
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
// [BUG] exporting as EFU should not use localized dates (only CSV should do this). EFU should be raw FILETIMEs.
// add a -max-path option. (limit paths to 259 chars -use shortpaths if we exceed.)
// add a -short-full-path option.
// -error-on-no-results alias
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

//#define ES_VERSION L"1.0.0.0" // initial release
//#define ES_VERSION L"1.0.0.1" // fixed command line interpreting '-' as a switch inside text.
//#define ES_VERSION L"1.0.0.2" // convert unicode to same code page as console.
//#define ES_VERSION L"1.0.0.3" // removed es_write console because it has no support for piping.
//#define ES_VERSION L"1.0.0.4" // added ChangeWindowMessageFilterEx (if available) for admin/user support.
//#define ES_VERSION L"1.1.0.1a" // Everything 1.4.1 support for size, dates etc.
//#define ES_VERSION L"1.1.0.2a" // -pause, -more, -hide-empty-search-results, -empty-search-help, -save-settings, -clear-settings, -path, -parent-path, -parent -no-*switches*, localized dates
//#define ES_VERSION L"1.1.0.3a" // error levels added, -timeout
//#define ES_VERSION L"1.1.0.4a" // -pause improvements, -set-run-count, -inc-run-count, -get-run-count, allow -name -size -date to re-order columns even when they are specified in settings, with -more ENTER=show one line, Q=Quit, ESC=Escape., column alignment improvements.
//#define ES_VERSION L"1.1.0.5a" // /a[RASH] attributes added, -more/-pause ignored for no results, -sort-size, -sort-size-ascending and -sort-size-descending.
//#define ES_VERSION L"1.1.0.6a" // added -csv, -efu, -txt, -m3u and -m3u8 to set the display format (no exporting), added -export-m3u and -export-m3u8.
//#define ES_VERSION L"1.1.0.7" // Everything 1.4 release.
//#define ES_VERSION L"1.1.0.8" // removed -highligh-name, -highligh-path and -highligh-full-path-name. fixed date widths, switched to ini settings.
//#define ES_VERSION L"1.1.0.9" // fixed an issue with checking if the database is loaded, exporting as efu only exports indexed information now, use -size, -date-modified, -date-created or -attributes to override., folder size is also exported now.
//#define ES_VERSION L"1.1.0.10" // -r now takes a parameter, and Everything handles escaping quotes.
//#define ES_VERSION L"1.1.0.11" // added -date-format
//#define ES_VERSION L"1.1.0.12" // added -get-result-count
//#define ES_VERSION L"1.1.0.13" // added -cd -removed -cd -added -ipc1 -ipc2 -output errors to std_error
//#define ES_VERSION L"1.1.0.14" // updated help (thanks to NotNull) and fixed -? -h -help errors
//#define ES_VERSION L"1.1.0.15" // updated help (thanks to NotNull) 
//#define ES_VERSION L"1.1.0.16" // added -no-header, added -double-quote, added -version, added -get-everything-version
//#define ES_VERSION L"1.1.0.17" // added -no-result-error
//#define ES_VERSION L"1.1.0.18" // added -get-total-size
//#define ES_VERSION L"1.1.0.19" // inc-run-count/set-run-count allow -instance
//#define ES_VERSION L"1.1.0.20" // fix ALT in pause mode causing page down.
//#define ES_VERSION L"1.1.0.21" // added TSV support.
//#define ES_VERSION L"1.1.0.22" // fixed an issue with WideCharToMultiByte using the wrong buffer size.
//#define ES_VERSION L"1.1.0.23" // fixed an issue with instance name not being initialized.
//#define ES_VERSION L"1.1.0.24" // fixed an issue with converting ini utf-8 to wchar text.
//#define ES_VERSION L"1.1.0.25" // added -exit and -reindex -utf8-bom
//#define ES_VERSION L"1.1.0.26" // fixed an issue with exporting UTF-16 surrogate pairs.
//#define ES_VERSION L"1.1.0.27" // improved WriteFile performance. (added es_buf_t) -added /cp 65001
//#define ES_VERSION L"1.1.0.28" // treat single - and double -- as literal (not as a switch)
//#define ES_VERSION L"1.1.0.29" // CSV/TSV -double quote, -search and -search*

// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
#define ES_VERSION L"1.1.0.29" 
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***
// *** YOU MUST MANUALLY UPDATE THE RESOURCE FILE WITH THE NEW VERSION ***

// errorlevel returned from ES.
#define ES_ERROR_SUCCESS					0 // no known error, search successful.
#define ES_ERROR_REGISTER_WINDOW_CLASS		1 // failed to register window class
#define ES_ERROR_CREATE_WINDOW				2 // failed to create listening window.
#define ES_ERROR_OUT_OF_MEMORY				3 // out of memory
#define ES_ERROR_EXPECTED_SWITCH_PARAMETER	4 // expected an additional command line option with the specified switch
#define ES_ERROR_CREATE_FILE				5 // failed to create export output file
#define ES_ERROR_UNKNOWN_SWITCH				6 // unknown switch.
#define ES_ERROR_SEND_MESSAGE				7 // failed to send Everything IPC a query.
#define ES_ERROR_IPC						8 // NO Everything IPC window.
#define ES_ERROR_NO_RESULTS					9 // No results found. Only set if -no-result-error is used

#define _DEBUG_STR(x) #x
#define DEBUG_STR(x) _DEBUG_STR(x)
//#define DEBUG_FIXME(x) __pragma(message(__FILE__ "(" DEBUG_STR(__LINE__) ") : FIXME: "x))
#define DEBUG_FIXME(x) __pragma(message(__FILE__ "(" DEBUG_STR(__LINE__) ") : FIXME: "x))

#ifdef _DEBUG
#include <assert.h>	// assert()
#else
#define assert(exp)
#endif

DEBUG_FIXME("___UPDATE YEAR IN RESOURCE___")

// compiler options
#pragma warning(disable : 4996) // deprecation
#pragma warning(disable : 4133) // warning C4133: 'function' : incompatible types - from 'CHAR *' to 'const wchar_t *'

#include <windows.h>
#include <stdio.h>

// IPC Header from the Everything SDK:
#include "..\..\sdk\ipc\everything_ipc.h"
#include "shlwapi.h" // Path functions

#define ES_COPYDATA_IPCTEST_QUERYCOMPLETEW		0
#define ES_COPYDATA_IPCTEST_QUERYCOMPLETE2W		1

#define ES_EXPORT_NONE		0
#define ES_EXPORT_CSV		1
#define ES_EXPORT_EFU		2
#define ES_EXPORT_TXT		3
#define ES_EXPORT_M3U		4
#define ES_EXPORT_M3U8		5
#define ES_EXPORT_TSV		6

#define ES_BUF_SIZE			(MAX_PATH * sizeof(wchar_t))
#define ES_WSTRING_SIZE		MAX_PATH
#define ES_EXPORT_BUF_SIZE	65536
#define ES_SEARCH_BUF_SIZE	(1 * 1024 * 1024)
#define ES_FILTER_BUF_SIZE	(1 * 1024 * 1024)
#define ES_WCHAR_BUF_STACK_SIZE		MAX_PATH
#define ES_UTF8_BUF_STACK_SIZE		MAX_PATH

#define ES_PAUSE_TEXT		"ESC=Quit; Up,Down,Left,Right,Page Up,Page Down,Home,End=Scroll"
#define ES_BLANK_PAUSE_TEXT	"                                                              "

#define ES_DWORD_MAX		0xffffffff
#define ES_UINT64_MAX		0xffffffffffffffffUI64

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

enum
{
	ES_COLUMN_FILENAME,
	ES_COLUMN_NAME,
	ES_COLUMN_PATH,
	ES_COLUMN_HIGHLIGHTED_FILENAME,
	ES_COLUMN_HIGHLIGHTED_NAME,
	ES_COLUMN_HIGHLIGHTED_PATH,
	ES_COLUMN_EXTENSION,
	ES_COLUMN_SIZE,
	ES_COLUMN_DATE_CREATED,
	ES_COLUMN_DATE_MODIFIED,
	ES_COLUMN_DATE_ACCESSED,
	ES_COLUMN_ATTRIBUTES,
	ES_COLUMN_FILE_LIST_FILENAME,
	ES_COLUMN_RUN_COUNT,
	ES_COLUMN_DATE_RUN,
	ES_COLUMN_DATE_RECENTLY_CHANGED,
	ES_COLUMN_TOTAL,
};

const wchar_t *es_column_names[] = 
{
	L"Filename",
	L"Name",
	L"Path",
	L"Filename", // highlighted
	L"Name", // highlighted
	L"Path", // highlighted
	L"Extension",
	L"Size",
	L"Date Created",
	L"Date Modified",
	L"Date Accessed",
	L"Attributes",
	L"File List Filename",
	L"Run Count",
	L"Date Run",
	L"Date Recently Changed",
};

const wchar_t *es_sort_names[] = 
{
	L"name",
	L"path",
	L"size",
	L"extension",
	L"ext",
	L"date-created",
	L"dc",
	L"date-modified",
	L"dm",
	L"attributes",
	L"attribs",
	L"attrib",
	L"file-list-file-name",
	L"run-count",
	L"recent-change",
	L"date-recently-changed",
	L"rc",
	L"drc",
	L"date-accessed",
	L"da",
	L"date-run",
	L"daterun",
};

#define ES_SORT_NAME_COUNT (sizeof(es_sort_names) / sizeof(const wchar_t *))

const DWORD es_sort_names_to_ids[] = 
{
	EVERYTHING_IPC_SORT_NAME_ASCENDING,
	EVERYTHING_IPC_SORT_PATH_ASCENDING,
	EVERYTHING_IPC_SORT_SIZE_DESCENDING,
	EVERYTHING_IPC_SORT_EXTENSION_ASCENDING,
	EVERYTHING_IPC_SORT_EXTENSION_ASCENDING,
	EVERYTHING_IPC_SORT_DATE_CREATED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_CREATED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_MODIFIED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_MODIFIED_DESCENDING,
	EVERYTHING_IPC_SORT_ATTRIBUTES_ASCENDING,
	EVERYTHING_IPC_SORT_ATTRIBUTES_ASCENDING,
	EVERYTHING_IPC_SORT_ATTRIBUTES_ASCENDING,
	EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_ASCENDING,
	EVERYTHING_IPC_SORT_RUN_COUNT_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_ACCESSED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_ACCESSED_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_RUN_DESCENDING,
	EVERYTHING_IPC_SORT_DATE_RUN_DESCENDING,
};

#define MSGFLT_ALLOW		1

typedef struct tagCHANGEFILTERSTRUCT 
{
	DWORD cbSize;
	DWORD ExtStatus;
}CHANGEFILTERSTRUCT, *PCHANGEFILTERSTRUCT;

typedef unsigned __int64 ES_UINT64;
typedef BYTE ES_UTF8;

typedef struct es_buf_s
{
	void *buf;
	BYTE stackbuf[ES_BUF_SIZE];
	
}es_buf_t;

// a dyanimcally sized wchar string
// has some stack space to avoid memory allocations.
typedef struct es_wchar_buf_s
{
	// pointer to wchar string data.
	// buf is NULL terminated.
	wchar_t *buf;
	
	// length of buf in wide chars.
	// does not include the null terminator.
	SIZE_T length_in_wchars;
	
	// size of the buffer in wide chars
	// includes room for the null terminator.
	SIZE_T size_in_wchars;
	
	// some stack for us before we need to allocate memory from the system.
	wchar_t stack_buf[ES_WCHAR_BUF_STACK_SIZE];
	
}es_wchar_buf_t;

// a dyanimcally sized UTF-8 string
// has some stack space to avoid memory allocations.
typedef struct es_utf8_buf_s
{
	// pointer to UTF-8 string data.
	// buf is NULL terminated.
	ES_UTF8 *buf;

	// length of buf in bytes.
	// does not include the null terminator.
	SIZE_T length_in_bytes;

	// size of the buffer in bytes
	// includes room for the null terminator.
	SIZE_T size_in_bytes;

	// some stack for us before we need to allocate memory from the system.
	ES_UTF8 stack_buf[ES_UTF8_BUF_STACK_SIZE];
	
}es_utf8_buf_t;

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

typedef struct _everything3_dimensions_s
{
	DWORD width;
	DWORD height;
	
}EVERYTHING3_DIMENSIONS;

typedef struct _EVERYTHING3_UINT128
{
	union
	{
		struct
		{
			ES_UINT64 lo_uint64;
			ES_UINT64 hi_uint64;
		};

		struct
		{
			DWORD lo_uint64_lo_dword;
			DWORD lo_uint64_hi_dword;
			DWORD hi_uint64_lo_dword;
			DWORD hi_uint64_hi_dword;
		};
	};
	
}EVERYTHING3_UINT128;

void DECLSPEC_NORETURN es_fatal(int error_code);
void es_write(const wchar_t *text);
void es_fwrite(const wchar_t *text);
void es_fwrite_n(const wchar_t *text,int wlen);
void es_write_wchar(wchar_t ch);
void es_write_highlighted(const wchar_t *text);
void es_write_UINT64(ES_UINT64 value);
void es_write_dword(DWORD value);
int es_wstring_to_int(const wchar_t *s);
int es_wstring_length(const wchar_t *s);
void es_format_number(wchar_t *buf,ES_UINT64 number);
int es_sendquery(HWND hwnd);
int es_sendquery2(HWND hwnd);
int es_sendquery3(void);
int es_compare_list_items(const void *a,const void *b);
void es_write_result_count(DWORD result_count);
void es_listresultsW(EVERYTHING_IPC_LIST *list);
void es_listresults2W(EVERYTHING_IPC_LIST2 *list,int index_start,int count);
void es_write_total_size(EVERYTHING_IPC_LIST2 *list,int count);
LRESULT __stdcall es_window_proc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
void es_help(void);
HWND es_find_ipc_window(void);
void es_wstring_copy(wchar_t *buf,const wchar_t *s);
void es_wstring_cat(wchar_t *buf,const wchar_t *s);
int es_check_param(wchar_t *param,const wchar_t *s);
int es_wstring_compare_param(wchar_t *param,const wchar_t *s);
void es_set_sort_ascending(void);
void es_set_sort_descending(void);
int es_find_column(int type);
void es_add_column(int type);
void *es_get_column_data(EVERYTHING_IPC_LIST2 *list,int index,int type);
void es_format_dir(wchar_t *buf);
void es_format_size(wchar_t *buf,ES_UINT64 size);
void es_format_filetime(wchar_t *buf,ES_UINT64 filetime);
void es_wstring_print_UINT64(wchar_t *buf,ES_UINT64 number);
void es_format_attributes(wchar_t *buf,DWORD attributes);
int es_filetime_to_localtime(SYSTEMTIME *localst,ES_UINT64 ft);
void es_format_leading_space(wchar_t *buf,int size,int ch);
void es_space_to_width(wchar_t *buf,int wide);
void es_format_run_count(wchar_t *buf,DWORD run_count);
int es_check_param_param(wchar_t *param,const wchar_t *s);
void es_flush(void);
void *es_mem_alloc(uintptr_t size);
void es_mem_free(void *ptr);
void es_fwrite_csv_string(const wchar_t *s);
void es_fwrite_csv_string_with_optional_quotes(int is_always_double_quote,int separator_ch,const wchar_t *s);
const wchar_t *es_get_command_argv(const wchar_t *command_line);
const wchar_t *es_get_argv(const wchar_t *command_line);
const wchar_t *es_skip_ws(const wchar_t *p);
int es_is_ws(const wchar_t c);
void es_set_color(int id);
void es_set_column_wide(int id);
void es_fill(int count,int utf8ch);
void es_wstring_cat_UINT64(wchar_t *buf,ES_UINT64 qw);
int es_is_valid_key(INPUT_RECORD *ir);
void es_ini_write_int(const char *name,int value,const char *filename);
void es_ini_write_string(const char *name,const wchar_t *value,const char *filename);
void es_save_settings(void);
int es_ini_read_int(const char *name,int *pvalue,const char *filename);
int es_ini_read_string(const char *name,wchar_t *pvalue,const char *filename);
void es_load_settings(void);
void es_remove_column(type);
void es_append_filter(const wchar_t *filter);
void es_wbuf_cat(wchar_t *buf,int max,const wchar_t *s);
wchar_t *es_wstring_alloc(const wchar_t *s);
void es_do_run_history_command(void);
int es_check_sorts(void);
int es_get_ini_filename(char *buf);
static void es_wait_for_db_loaded(void);
static void es_wait_for_db_not_busy(void);
static void *es_buf_init(es_buf_t *cbuf,uintptr_t size);
static void es_buf_kill(es_buf_t *cbuf);
static int es_is_literal_switch(const wchar_t *s);
static int _es_should_quote(int separator_ch,const wchar_t *s);
static int _es_is_unbalanced_quotes(const wchar_t *s);
static void es_wchar_buf_init(es_wchar_buf_t *wcbuf);
static void es_wchar_buf_kill(es_wchar_buf_t *wcbuf);
static void es_wchar_buf_empty(es_wchar_buf_t *wcbuf);
static void es_wchar_buf_grow_size(es_wchar_buf_t *wcbuf,SIZE_T length_in_wchars);
static void es_wchar_buf_grow_length(es_wchar_buf_t *wcbuf,SIZE_T length_in_wchars);
static void es_wchar_buf_get_pipe_name(es_wchar_buf_t *wcbuf,const wchar_t *instance_name);
static SIZE_T es_safe_size_add(SIZE_T a,SIZE_T b);
static wchar_t *es_get_pipe_name(wchar_t *buf,const wchar_t *instance_name);
static wchar_t *es_wchar_string_cat_wchar_string_no_null_terminate(wchar_t *buf,wchar_t *current_d,const wchar_t *s);
static BYTE *es_copy_len_vlq(BYTE *buf,SIZE_T value);
static BYTE *es_copy_dword(BYTE *buf,DWORD value);
static BYTE *es_copy_uint64(BYTE *buf,ES_UINT64 value);
static BYTE *es_copy_size_t(BYTE *buf,SIZE_T value);
static void *es_copy_memory(void *dst,const void *src,SIZE_T size);
static void *es_zero_memory(void *dst,SIZE_T size);
static BOOL es_write_pipe_data(HANDLE h,const void *in_data,SIZE_T in_size);
static BOOL es_write_pipe_message(HANDLE h,DWORD code,const void *in_data,SIZE_T in_size);
static void _everything3_stream_read_data(_everything3_stream_t *stream,void *data,SIZE_T size);
static BYTE _everything3_stream_read_byte(_everything3_stream_t *stream);
static WORD _everything3_stream_read_word(_everything3_stream_t *stream);
static DWORD _everything3_stream_read_dword(_everything3_stream_t *stream);
static ES_UINT64 _everything3_stream_read_uint64(_everything3_stream_t *stream);
static SIZE_T _everything3_stream_read_size_t(_everything3_stream_t *stream);
static SIZE_T _everything3_stream_read_len_vlq(_everything3_stream_t *stream);
static void es_utf8_buf_init(es_utf8_buf_t *wcbuf);
static void es_utf8_buf_kill(es_utf8_buf_t *wcbuf);
static void es_utf8_buf_empty(es_utf8_buf_t *wcbuf);
static void es_utf8_buf_grow_size(es_utf8_buf_t *wcbuf,SIZE_T size_in_bytes);
static void es_utf8_buf_grow_length(es_utf8_buf_t *wcbuf,SIZE_T length_in_bytes);
static BOOL es_read_pipe(HANDLE pipe_handle,void *buf,SIZE_T buf_size);
static ES_UTF8 *es_utf8_string_copy_wchar_string(ES_UTF8 *buf,const wchar_t *ws);
static void es_utf8_buf_copy_wchar_string(es_utf8_buf_t *cbuf,const wchar_t *ws);

//TODO: es_sort_property_id
int es_sort = EVERYTHING_IPC_SORT_NAME_ASCENDING;
int es_sort_ascending = 0; // 0 = default, >0 = ascending, <0 = descending
EVERYTHING_IPC_LIST *es_sort_list;
HMODULE es_user32_hdll = 0;
wchar_t *es_instance = 0;
BOOL (WINAPI *es_pChangeWindowMessageFilterEx)(HWND hWnd,UINT message,DWORD action,PCHANGEFILTERSTRUCT pChangeFilterStruct) = 0;
int es_highlight_color = FOREGROUND_GREEN|FOREGROUND_INTENSITY;
int es_highlight = 0;
int es_match_whole_word = 0;
int es_match_path = 0;
int es_match_case = 0;
int es_match_diacritics = 0;
int es_exit_everything = 0;
int es_reindex = 0;
int es_save_db = 0;
int es_export = ES_EXPORT_NONE;
HANDLE es_export_file = INVALID_HANDLE_VALUE;
unsigned char *es_export_buf = 0;
unsigned char *es_export_p;
int es_export_remaining = 0;
int es_numcolumns = 0;
int es_columns[ES_COLUMN_TOTAL];
int es_size_leading_zero = 0;
int es_run_count_leading_zero = 0;
int es_digit_grouping = 1;
DWORD es_offset = 0;
//TODO: size_t
DWORD es_max_results = EVERYTHING_IPC_ALLRESULTS;
DWORD es_ret = ES_ERROR_SUCCESS;
wchar_t *es_connect = 0;
wchar_t *es_search = 0;
wchar_t *es_filter = 0;
wchar_t *es_argv = 0;
WORD es_column_color[ES_COLUMN_TOTAL];
WORD es_column_color_is_valid[ES_COLUMN_TOTAL] = {0};
WORD es_last_color;
int es_is_last_color = 0;
int es_size_format = 1; // 0 = auto, 1=bytes, 2=kb
int es_date_format = 0; // 0 = system format, 1=iso-8601 (as local time), 2=filetime in decimal, 3=iso-8601 (in utc)
CHAR_INFO *es_cibuf = 0;
int es_max_wide = 0;
int es_console_wide = 80;
int es_console_high = 25;
int es_console_size_high = 25;
int es_console_window_x = 0;
int es_console_window_y = 0;
int es_pause = 0; 
int es_last_page_start = 0; 
int es_cibuf_attributes = 0x07;
int es_cibuf_hscroll = 0;
int es_cibuf_x = 0;
int es_cibuf_y = 0;
int es_empty_search_help = 0;
int es_hide_empty_search_results = 0;
int es_save = 0;
HWND es_everything_hwnd = 0;
DWORD es_timeout = 0;
int es_get_result_count = 0; // 1 = show result count only
int es_get_total_size = 0; // 1 = calculate total result size, only display this total size and exit.
int es_no_result_error = 0; // 1 = set errorlevel if no results found.
HANDLE es_output_handle = 0;
UINT es_cp = 0; // current code page
int es_output_is_file = 1; // default to file, unless we can get the console mode.
int es_default_attributes = 0x07;
void *es_run_history_data = 0; // run count command
DWORD es_run_history_count = 0; // run count command
int es_run_history_command = 0;
int es_run_history_size = 0;
DWORD es_ipc_version = 0xffffffff;
int es_header = 1;
int es_double_quote = 0; // always use double quotes for filenames.
int es_csv_double_quote = 1; // always use double quotes for CSV for consistancy.
int es_get_everything_version = 0;
int es_utf8_bom = 0;

int es_column_widths[ES_COLUMN_TOTAL] = 
{
	47, // ES_COLUMN_FILENAME,
	30, // ES_COLUMN_NAME,
	47, // ES_COLUMN_PATH,
	47, // ES_COLUMN_HIGHLIGHTED_FILENAME,
	30, // ES_COLUMN_HIGHLIGHTED_NAME,
	47, // ES_COLUMN_HIGHLIGHTED_PATH,
	4, // ES_COLUMN_EXTENSION,
	14, // ES_COLUMN_SIZE,
	16, // ES_COLUMN_DATE_CREATED,
	16, // ES_COLUMN_DATE_MODIFIED,
	16, // ES_COLUMN_DATE_ACCESSED,
	4, // ES_COLUMN_ATTRIBUTES,
	30, // ES_COLUMN_FILE_LIST_FILENAME,
	6, // ES_COLUMN_RUN_COUNT,
	16, // ES_COLUMN_DATE_RUN,
	16, // ES_COLUMN_DATE_RECENTLY_CHANGED,
};

int es_column_is_right_aligned[ES_COLUMN_TOTAL] = 
{
	0, // ES_COLUMN_FILENAME,
	0, // ES_COLUMN_NAME,
	0, // ES_COLUMN_PATH,
	0, // ES_COLUMN_HIGHLIGHTED_FILENAME,
	0, // ES_COLUMN_HIGHLIGHTED_NAME,
	0, // ES_COLUMN_HIGHLIGHTED_PATH,
	0, // ES_COLUMN_EXTENSION,
	1, // ES_COLUMN_SIZE,
	0, // ES_COLUMN_DATE_CREATED,
	0, // ES_COLUMN_DATE_MODIFIED,
	0, // ES_COLUMN_DATE_ACCESSED,
	0, // ES_COLUMN_ATTRIBUTES,
	0, // ES_COLUMN_FILE_LIST_FILENAME,
	1, // ES_COLUMN_RUN_COUNT,
	0, // ES_COLUMN_DATE_RUN,
	0, // ES_COLUMN_DATE_RECENTLY_CHANGED,
};

// get the length in wchars from a wchar string.
int es_wstring_length(const wchar_t *s)
{
	const wchar_t *p;
	
	p = s;
	while(*p)
	{
		p++;
	}
	
	return (int)(p - s);
}

// get the length in wchars from highlighted string
// skips over the '*' parts
int es_wstring_highlighted_length(const wchar_t *s)
{
	const wchar_t *p;
	int len;
	
	len = 0;
	p = s;
	while(*p)
	{
		if (*p == '*')
		{
			p++;
			
			if (*p == '*')
			{
				len++;
				p++;
			}
		}
		else
		{
			p++;
			len++;
		}
	}
	
	return len;
}

// query everything with search string
int es_sendquery(HWND hwnd)
{
	EVERYTHING_IPC_QUERY *query;
	int len;
	int size;
	COPYDATASTRUCT cds;
	int ret;
	es_buf_t cbuf;
	
	ret = 0;
	
	len = es_wstring_length(es_search);
	
	size = sizeof(EVERYTHING_IPC_QUERY) - sizeof(WCHAR) + len*sizeof(WCHAR) + sizeof(WCHAR);
	
	query = es_buf_init(&cbuf,size);

	if (es_get_result_count)
	{
		es_max_results = 0;
	}
	
	query->max_results = es_max_results;
	query->offset = 0;
	query->reply_copydata_message = ES_COPYDATA_IPCTEST_QUERYCOMPLETEW;
	query->search_flags = (es_match_case?EVERYTHING_IPC_MATCHCASE:0) | (es_match_diacritics?EVERYTHING_IPC_MATCHACCENTS:0) | (es_match_whole_word?EVERYTHING_IPC_MATCHWHOLEWORD:0) | (es_match_path?EVERYTHING_IPC_MATCHPATH:0);
	query->reply_hwnd = (DWORD)(uintptr_t)hwnd;
	es_copy_memory(query->search_string,es_search,(len+1)*sizeof(WCHAR));

	cds.cbData = size;
	cds.dwData = EVERYTHING_IPC_COPYDATAQUERY;
	cds.lpData = query;

	if (SendMessage(es_everything_hwnd,WM_COPYDATA,(WPARAM)hwnd,(LPARAM)&cds) == TRUE)
	{
		ret = 1;
	}

	es_buf_kill(&cbuf);

	return ret;
}

// query everything with search string
int es_sendquery2(HWND hwnd)
{
	EVERYTHING_IPC_QUERY2 *query;
	int len;
	int size;
	COPYDATASTRUCT cds;
	int ret;
	DWORD request_flags;
	es_buf_t cbuf;
	
	ret = 0;

	len = es_wstring_length(es_search);
	
	size = sizeof(EVERYTHING_IPC_QUERY2) + ((len + 1) * sizeof(WCHAR));

	query = es_buf_init(&cbuf,size);

	request_flags = 0;
	
	{
		int columni;
		
		for(columni=0;columni<es_numcolumns;columni++)
		{
			switch(es_columns[columni])
			{
				case ES_COLUMN_FILENAME:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_FULL_PATH_AND_NAME;
					break;
					
				case ES_COLUMN_NAME:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_NAME;
					break;
					
				case ES_COLUMN_PATH:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_PATH;
					break;
					
				case ES_COLUMN_HIGHLIGHTED_FILENAME:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_FULL_PATH_AND_NAME;
					break;
					
				case ES_COLUMN_HIGHLIGHTED_NAME:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_NAME;
					break;
					
				case ES_COLUMN_HIGHLIGHTED_PATH:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_PATH;
					break;
					
				case ES_COLUMN_EXTENSION:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_EXTENSION;
					break;

				case ES_COLUMN_SIZE:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_SIZE;
					break;

				case ES_COLUMN_DATE_CREATED:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_CREATED;
					break;

				case ES_COLUMN_DATE_MODIFIED:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_MODIFIED;
					break;

				case ES_COLUMN_DATE_ACCESSED:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_ACCESSED;
					break;

				case ES_COLUMN_ATTRIBUTES:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_ATTRIBUTES;
					break;

				case ES_COLUMN_FILE_LIST_FILENAME:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_FILE_LIST_FILE_NAME;
					break;

				case ES_COLUMN_RUN_COUNT:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_RUN_COUNT;
					break;

				case ES_COLUMN_DATE_RUN:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_RUN;
					break;

				case ES_COLUMN_DATE_RECENTLY_CHANGED:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_RECENTLY_CHANGED;
					break;
			}
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
	query->reply_hwnd = (DWORD)(uintptr_t)hwnd;
	query->request_flags = request_flags;
	query->sort_type = es_sort;
	es_copy_memory(query+1,es_search,(len + 1) * sizeof(WCHAR));

	cds.cbData = size;
	cds.dwData = EVERYTHING_IPC_COPYDATA_QUERY2;
	cds.lpData = query;

	if (SendMessage(es_everything_hwnd,WM_COPYDATA,(WPARAM)hwnd,(LPARAM)&cds) == TRUE)
	{
		ret = 1;
	}

	es_buf_kill(&cbuf);

	return ret;
}

int es_sendquery3(void)
{
	int ret;
	es_wchar_buf_t pipe_name_wcbuf;
	HANDLE pipe_handle;
	
	ret = 0;
	es_wchar_buf_init(&pipe_name_wcbuf);

	es_wchar_buf_get_pipe_name(&pipe_name_wcbuf,es_instance);
		
	pipe_handle = CreateFile(pipe_name_wcbuf.buf,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,0);
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		DWORD search_flags;
		es_utf8_buf_t search_cbuf;
		BYTE *packet_data;
		SIZE_T packet_size;
		SIZE_T sort_count;
		SIZE_T property_request_count;

		es_utf8_buf_init(&search_cbuf);
	
		es_utf8_buf_copy_wchar_string(&search_cbuf,es_search);

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
		packet_size = es_safe_size_add(packet_size,(SIZE_T)es_copy_len_vlq(NULL,search_cbuf.length_in_bytes));
		
		// search_text
		packet_size = es_safe_size_add(packet_size,search_cbuf.length_in_bytes);

		// view port offset
		packet_size = es_safe_size_add(packet_size,sizeof(SIZE_T));
		packet_size = es_safe_size_add(packet_size,sizeof(SIZE_T));

		// sort
		packet_size = es_safe_size_add(packet_size,(SIZE_T)es_copy_len_vlq(NULL,sort_count));
//		packet_size = es_safe_size_add(packet_size,sort_count * sizeof(es_search_sort_t));

		// property request 
		packet_size = es_safe_size_add(packet_size,(SIZE_T)es_copy_len_vlq(NULL,property_request_count));
//		packet_size = es_safe_size_add(packet_size,property_request_count * sizeof(es_search_property_request_t));
		
		packet_data = es_mem_alloc(packet_size);
		if (packet_data)
		{
			BYTE *packet_d;
			
			packet_d = packet_data;
			
			// search flags
			packet_d = es_copy_dword(packet_d,search_flags);
			
			// search text
			packet_d = es_copy_len_vlq(packet_d,search_cbuf.length_in_bytes);
			packet_d = es_copy_memory(packet_d,search_cbuf.buf,search_cbuf.length_in_bytes);

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
			assert((packet_d - packet_data) == packet_size);
			
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
					
					assert(sizeof(_everything3_result_list_property_request_t) <= 12);

					property_request_size = es_safe_size_add(property_request_size,property_request_size); // x2
					property_request_size = es_safe_size_add(property_request_size,property_request_size); // x4
					property_request_size = es_safe_size_add(property_request_size,property_request_size); // x8
					property_request_size = es_safe_size_add(property_request_size,property_request_size); // x16 (over allocate)
					
					property_request_array = es_mem_alloc(property_request_size);
					
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
					es_utf8_buf_t property_text_cbuf;

					es_utf8_buf_init(&property_text_cbuf);
					
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
									
									es_utf8_buf_grow_length(&property_text_cbuf,len);
									
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
												
												es_utf8_buf_grow_length(&property_text_cbuf,len);
												
												_everything3_stream_read_data(&stream,property_text_cbuf.buf,len);
												
												property_text_cbuf.buf[len] = 0;
												
												printf("ITEM PROPERTY %s\n",property_text_cbuf.buf);
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
												
												es_utf8_buf_grow_length(&property_text_cbuf,len);
												
												_everything3_stream_read_data(&stream,property_text_cbuf.buf,len);
												
												property_text_cbuf.buf[len] = 0;
											}

											break;

										case EVERYTHING3_PROPERTY_VALUE_TYPE_BLOB16:

											{
												WORD len;
												
												len = _everything3_stream_read_word(&stream);
												
												es_utf8_buf_grow_length(&property_text_cbuf,len);
												
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

					es_utf8_buf_kill(&property_text_cbuf);
				}
				
				if (!stream.is_error)
				{
					printf("OK\n");
					
					ret = 1;
				}
				
				if (property_request_array)
				{
					es_mem_free(property_request_array);
				}
				
				if (stream.buf)
				{
					es_mem_free(stream.buf);
				}
			}
			
			es_mem_free(packet_data);
		}
	
		es_utf8_buf_kill(&search_cbuf);

		CloseHandle(pipe_handle);
	}

	es_wchar_buf_kill(&pipe_name_wcbuf);
	
	return ret;
}

int es_compare_list_items(const void *a,const void *b)
{
	int i;
	
	i = wcsicmp(EVERYTHING_IPC_ITEMPATH(es_sort_list,a),EVERYTHING_IPC_ITEMPATH(es_sort_list,b));
	
	if (!i)
	{
		return wcsicmp(EVERYTHING_IPC_ITEMFILENAME(es_sort_list,a),EVERYTHING_IPC_ITEMFILENAME(es_sort_list,b));
	}
	else
	if (i > 0)
	{
		return 1;
	}
	else
	{
		return -1;
	}
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
			msg = "Error 1: Failed to register window class.\r\n";
			break;
			
		case ES_ERROR_CREATE_WINDOW:
			msg = "Error 2: Failed to create window.\r\n";
			break;
			
		case ES_ERROR_OUT_OF_MEMORY:
			msg = "Error 3: Out of memory.\r\n";
			break;
			
		case ES_ERROR_EXPECTED_SWITCH_PARAMETER:
			msg = "Error 4: Expected switch parameter.\r\n";
			// this error is permanent, show help:
			show_help = 1;
			break;
			
		case ES_ERROR_CREATE_FILE:
			msg = "Error 5: Failed to create output file.\r\n";
			break;
			
		case ES_ERROR_UNKNOWN_SWITCH:
			msg = "Error 6: Unknown switch.\r\n";
			// this error is permanent, show help:
			show_help = 1;
			break;
			
		case ES_ERROR_SEND_MESSAGE:
			msg = "Error 7: Unable to send IPC message.\r\n";
			break;
			
		case ES_ERROR_IPC:
			msg = "Error 8: Everything IPC window not found. Please make sure Everything is running.\r\n";
			break;
			
		case ES_ERROR_NO_RESULTS:
			msg = "Error 9: No results found.\r\n";
			break;
	}
	
	if (msg)
	{
		DWORD numwritten;
		
		// msg is ASCII only.
		WriteFile(GetStdHandle(STD_ERROR_HANDLE),msg,(int)strlen(msg),&numwritten,0);
	}
	
	if (show_help)
	{
		es_help();
	}

	ExitProcess(error_code);
}

void es_write(const wchar_t *text)
{
	int wlen;
	
	wlen = es_wstring_length(text);
	
	if (es_cibuf)
	{
		int i;
		
		for(i=0;i<wlen;i++)
		{
			if (i + es_cibuf_x >= es_console_wide)
			{
				break;
			}

			if (i + es_cibuf_x >= 0)
			{
				es_cibuf[i+es_cibuf_x].Attributes = es_cibuf_attributes;
				es_cibuf[i+es_cibuf_x].Char.UnicodeChar = text[i];
			}
		}
		
		es_cibuf_x += wlen;
	}
	else
	{
		// pipe?
		if (es_output_is_file)
		{
			int len;
			
			len = WideCharToMultiByte(es_cp,0,text,wlen,0,0,0,0);
			if (len)
			{
				DWORD numwritten;
				es_buf_t cbuf;

				es_buf_init(&cbuf,len);

				WideCharToMultiByte(es_cp,0,text,wlen,cbuf.buf,len,0,0);
				
				WriteFile(es_output_handle,cbuf.buf,len,&numwritten,0);
				
				es_buf_kill(&cbuf);
			}
		}
		else
		{
			DWORD numwritten;
			
			WriteConsoleW(es_output_handle,text,wlen,&numwritten,0);
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
		es_buf_t cbuf;
		unsigned char *p;
		DWORD numwritten;
		int i;
		
		es_buf_init(&cbuf,count);

		p = cbuf.buf;
		
		for(i=0;i<count;i++)
		{
			*p++ = utf8ch;
		}

 		WriteFile(es_output_handle,cbuf.buf,count,&numwritten,0);
		
		es_buf_kill(&cbuf);
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
			es_buf_t cbuf;
			
			cp = CP_UTF8;

			if (es_export == ES_EXPORT_M3U)		
			{
				cp = CP_ACP;
			}
			
			len = WideCharToMultiByte(cp,0,text,wlen,0,0,0,0);
			
			es_buf_init(&cbuf,len);

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
			es_copy_memory(es_export_p,cbuf.buf,len);
			es_export_p += len;

	copied:
			
			es_buf_kill(&cbuf);
		}
		else
		{
			es_write(text);
		}
	}
}

void es_fwrite(const wchar_t *text)
{
	es_fwrite_n(text,-1);
}

void es_fwrite_csv_string(const wchar_t *s)
{
	const wchar_t *start;
	const wchar_t *p;
	
	es_fwrite(L"\"");
	
	// escape double quotes with double double quotes.
	start = s;
	p = s;
	
	while(*p)
	{
		if (*p == '"')
		{
			es_fwrite_n(start,(int)(p-start));

			// escape double quotes with double double quotes.
			es_fwrite(L"\"");
			es_fwrite(L"\"");
			
			start = p + 1;
		}
		
		p++;
	}

	es_fwrite_n(start,(int)(p-start));
	
	es_fwrite(L"\"");
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
			es_fwrite(s);
			
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
		
			if (es_output_is_file)
			{
				int mblen;
				
				mblen = WideCharToMultiByte(es_cp,0,start,wlen,0,0,0,0);
				if (mblen)
				{
					DWORD numwritten;
					es_buf_t cbuf;
					
					es_buf_init(&cbuf,mblen);
					
					WideCharToMultiByte(es_cp,0,start,wlen,cbuf.buf,mblen,0,0);
					
					WriteFile(es_output_handle,cbuf.buf,mblen,&numwritten,0);
					
					es_buf_kill(&cbuf);
				}			
			}
			else
			{
				DWORD numwritten;
				
				WriteConsoleW(es_output_handle,start,wlen,&numwritten,0);
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
	wchar_t buf[ES_WSTRING_SIZE];
	wchar_t *d;
	
	d = buf + ES_WSTRING_SIZE;
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
	
	es_write(d);
}

void es_write_dword(DWORD value)
{
	es_write_UINT64(value);
}

void es_write_result_count(DWORD result_count)
{
	es_write_dword(result_count);
	es_write(L"\r\n");
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
		DWORD i;
		
		if (es_sort == EVERYTHING_IPC_SORT_PATH_ASCENDING)
		{
			es_sort_list = list;
			qsort(list->items,list->numitems,sizeof(EVERYTHING_IPC_ITEM),es_compare_list_items);
		}
			
		
		for(i=0;i<list->numitems;i++)
		{
			if (list->items[i].flags & EVERYTHING_IPC_DRIVE)
			{
				es_write(EVERYTHING_IPC_ITEMFILENAME(list,&list->items[i]));
				es_write(L"\r\n");
			}
			else
			{
				es_write(EVERYTHING_IPC_ITEMPATH(list,&list->items[i]));
				es_write(L"\\");
				es_write(EVERYTHING_IPC_ITEMFILENAME(list,&list->items[i]));
				es_write(L"\r\n");
			}
		}
	}
	
	PostQuitMessage(0);
}

void es_listresults2W(EVERYTHING_IPC_LIST2 *list,int index_start,int count)
{
	DWORD i;
	EVERYTHING_IPC_ITEM2 *items;
	
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
		
		if ((es_export == ES_EXPORT_CSV) || (es_export == ES_EXPORT_TSV))
		{
			int columni;
			int csv_double_quote;
			int separator_ch;
			
			// Like Excel, avoid quoting unless we really need to with TSV
			// CSV should always quote for consistancy.
			csv_double_quote = (es_export == ES_EXPORT_CSV) ? es_csv_double_quote : es_double_quote;
			
			separator_ch = (es_export == ES_EXPORT_CSV) ? ',' : '\t';
			
			for(columni=0;columni<es_numcolumns;columni++)
			{
				void *data;
				
				if (columni)
				{
					es_write_wchar(separator_ch);
				}
				
				data = es_get_column_data(list,i,es_columns[columni]);
				
				if (data)
				{
					switch(es_columns[columni])
					{
						case ES_COLUMN_NAME:
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
							
						case ES_COLUMN_PATH:
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
							
						case ES_COLUMN_FILENAME:
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
							
						case ES_COLUMN_HIGHLIGHTED_NAME:
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
							
						case ES_COLUMN_HIGHLIGHTED_PATH:
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
							
						case ES_COLUMN_HIGHLIGHTED_FILENAME:
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
							
						case ES_COLUMN_EXTENSION:
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
							
						case ES_COLUMN_SIZE:
						{
							wchar_t column_buf[ES_WSTRING_SIZE];
							
							if ((items[i].flags & EVERYTHING_IPC_FOLDER) && (*(ES_UINT64 *)data == 0xffffffffffffffffI64))
							{
								column_buf[0] = 0;
							}
							else
							{
								es_wstring_print_UINT64(column_buf,*(ES_UINT64 *)data);
							}
							
							es_fwrite(column_buf);
							
							break;
						}

						case ES_COLUMN_DATE_MODIFIED:
						{
							wchar_t column_buf[ES_WSTRING_SIZE];
							
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							es_fwrite(column_buf);
							break;
						}

						case ES_COLUMN_DATE_CREATED:
						{
							wchar_t column_buf[ES_WSTRING_SIZE];
							
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							es_fwrite(column_buf);
							break;
						}

						case ES_COLUMN_DATE_ACCESSED:
						{
							wchar_t column_buf[ES_WSTRING_SIZE];
							
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							es_fwrite(column_buf);
							break;
						}

						case ES_COLUMN_ATTRIBUTES:
						{
							wchar_t column_buf[ES_WSTRING_SIZE];
							
							es_format_attributes(column_buf,*(DWORD *)data);
							es_fwrite(column_buf);
							break;
						}

						case ES_COLUMN_FILE_LIST_FILENAME:
						{
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
						}
						
						case ES_COLUMN_RUN_COUNT:
						{
							wchar_t column_buf[ES_WSTRING_SIZE];
							
							es_wstring_print_UINT64(column_buf,*(DWORD *)data);
							es_fwrite(column_buf);
							break;
						}	
											
						case ES_COLUMN_DATE_RUN:
						{
							wchar_t column_buf[ES_WSTRING_SIZE];
							
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							es_fwrite(column_buf);
							break;
						}		
															
						case ES_COLUMN_DATE_RECENTLY_CHANGED:
						{
							wchar_t column_buf[ES_WSTRING_SIZE];
							
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							es_fwrite(column_buf);
							break;
						}						
					}
				}
			}

			es_fwrite(L"\r\n");
		}
		else
		if (es_export == ES_EXPORT_EFU)
		{
			void *data;
				
			// Filename
			data = es_get_column_data(list,i,ES_COLUMN_FILENAME);
			if (data)
			{
				es_fwrite_csv_string((wchar_t *)((char *)data+sizeof(DWORD)));
			}

			es_fwrite(L",");
			
			// size
			data = es_get_column_data(list,i,ES_COLUMN_SIZE);
			if (data)
			{
				wchar_t column_buf[ES_WSTRING_SIZE];
				
				if (*(ES_UINT64 *)data != 0xffffffffffffffffI64)
				{
					es_wstring_print_UINT64(column_buf,*(ES_UINT64 *)data);
					es_fwrite(column_buf);
				}
			}

			es_fwrite(L",");

			// date modified
			data = es_get_column_data(list,i,ES_COLUMN_DATE_MODIFIED);
			if (data)
			{
				if ((*(ES_UINT64 *)data != 0xffffffffffffffffI64))
				{
					wchar_t column_buf[ES_WSTRING_SIZE];
					
					es_wstring_print_UINT64(column_buf,*(ES_UINT64 *)data);
					es_fwrite(column_buf);
				}
			}

			es_fwrite(L",");

			// date created
			data = es_get_column_data(list,i,ES_COLUMN_DATE_CREATED);
			if (data)
			{
				if ((*(ES_UINT64 *)data != 0xffffffffffffffffI64))
				{
					wchar_t column_buf[ES_WSTRING_SIZE];
					
					es_wstring_print_UINT64(column_buf,*(ES_UINT64 *)data);
					es_fwrite(column_buf);
				}
			}

			es_fwrite(L",");

			// date created
			data = es_get_column_data(list,i,ES_COLUMN_ATTRIBUTES);
			if (data)
			{
				wchar_t column_buf[ES_WSTRING_SIZE];
						
				es_wstring_print_UINT64(column_buf,*(DWORD *)data);
				es_fwrite(column_buf);
			}
			else
			{
				DWORD file_attributes;
				wchar_t column_buf[ES_WSTRING_SIZE];
				
				// add file/folder attributes.
				if (items[i].flags & EVERYTHING_IPC_FOLDER)
				{
					file_attributes = FILE_ATTRIBUTE_DIRECTORY;
				}
				else
				{
					file_attributes = 0;
				}

				es_wstring_print_UINT64(column_buf,file_attributes);
				es_fwrite(column_buf);
			}

			es_fwrite(L"\r\n");
		}
		else
		if ((es_export == ES_EXPORT_TXT) || (es_export == ES_EXPORT_M3U) || (es_export == ES_EXPORT_M3U8))
		{
			void *data;
			
			data = es_get_column_data(list,i,ES_COLUMN_FILENAME);
			
			if (es_double_quote)
			{
				es_fwrite(L"\"");
			}
			
			if (data)
			{
				es_fwrite((wchar_t *)((char *)data+sizeof(DWORD)));
			}

			if (es_double_quote)
			{
				es_fwrite(L"\"");
			}
			
			es_fwrite(L"\r\n");
		}
		else
		{
			int columni;
			int did_set_color;
			int tot_column_text_len;
			int tot_column_width;
			
			tot_column_text_len = 0;
			tot_column_width = 0;

			for(columni=0;columni<es_numcolumns;columni++)
			{
				void *data;
				const wchar_t *column_text;
				int column_is_highlighted;
				int column_is_double_quote;
				wchar_t column_buf[ES_WSTRING_SIZE];
				
				if (columni)
				{
					es_write(L" ");
					tot_column_text_len++;
					tot_column_width++;
				}
				
				data = es_get_column_data(list,i,es_columns[columni]);

				es_is_last_color = 0;
				did_set_color = 0;
				
				column_text = L"";
				column_is_highlighted = 0;
				column_is_double_quote = 0;

				if (data)
				{
					if (!es_output_is_file)
					{
						if (es_column_color_is_valid[es_columns[columni]])
						{
							es_cibuf_attributes = es_column_color[es_columns[columni]];
							SetConsoleTextAttribute(es_output_handle,es_column_color[es_columns[columni]]);

							es_last_color = es_column_color[es_columns[columni]];
							es_is_last_color = 1;

							did_set_color = 1;
						}
					}

					switch(es_columns[columni])
					{
						case ES_COLUMN_NAME:
							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_double_quote = es_double_quote;
							break;
							
						case ES_COLUMN_PATH:
							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_double_quote = es_double_quote;
							break;
							
						case ES_COLUMN_FILENAME:
							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_double_quote = es_double_quote;
							break;
							
						case ES_COLUMN_HIGHLIGHTED_NAME:
							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_highlighted = 1;
							column_is_double_quote = es_double_quote;
							break;
							
						case ES_COLUMN_HIGHLIGHTED_PATH:
							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_highlighted = 1;
							column_is_double_quote = es_double_quote;
							break;
							
						case ES_COLUMN_HIGHLIGHTED_FILENAME:
							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_highlighted = 1;
							column_is_double_quote = es_double_quote;
							break;
							
						case ES_COLUMN_EXTENSION:
						{
							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							break;
						}
							
						case ES_COLUMN_SIZE:
						{
							
							if ((items[i].flags & EVERYTHING_IPC_FOLDER) && (*(ES_UINT64 *)data == 0xffffffffffffffffI64))
							{
								es_format_dir(column_buf);
							}
							else
							{
								es_format_size(column_buf,*(ES_UINT64 *)data);
							}

							column_text = column_buf;

							break;
						}

						case ES_COLUMN_DATE_CREATED:
						{
							es_format_filetime(column_buf,*(ES_UINT64 *)data);

							column_text = column_buf;
							break;
						}
						
						case ES_COLUMN_DATE_MODIFIED:
						{
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							column_text = column_buf;
							break;
						}
						
						case ES_COLUMN_DATE_ACCESSED:
						{
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							column_text = column_buf;
							break;
						}
						
						case ES_COLUMN_ATTRIBUTES:
						{
							es_format_attributes(column_buf,*(DWORD *)data);
							column_text = column_buf;
							break;
						}

						case ES_COLUMN_FILE_LIST_FILENAME:
						{
							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							break;
						}
						
						case ES_COLUMN_RUN_COUNT:
						{
							es_format_run_count(column_buf,*(DWORD *)data);
							column_text = column_buf;
							break;
						}	
											
						case ES_COLUMN_DATE_RUN:
						{
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							column_text = column_buf;
							break;
						}		
															
						case ES_COLUMN_DATE_RECENTLY_CHANGED:
						{
							es_format_filetime(column_buf,*(ES_UINT64 *)data);
							column_text = column_buf;
							break;
						}
					}

					//es_column_widths
					{
						int column_text_len;
						int fill;
						int column_width;
						int is_right_aligned;
						
						is_right_aligned = es_column_is_right_aligned[es_columns[columni]];
						
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
							column_text_len = es_wstring_highlighted_length(column_text);
						}
						else
						{
							column_text_len = es_wstring_length(column_text);
						}
						
						if (column_is_double_quote)
						{
							column_text_len += 2;
						}
						
						column_width = es_column_widths[es_columns[columni]];
						
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
									if (es_columns[columni] == ES_COLUMN_SIZE)
									{
										fillch = es_size_leading_zero ? '0' : ' ';
									}
									else
									if (es_columns[columni] == ES_COLUMN_RUN_COUNT)
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
							es_write(L"\"");
						}

						// write text.
						if (column_is_highlighted)
						{
							es_write_highlighted(column_text);
						}
						else
						{
							es_write(column_text);
						}
				
						if (column_is_double_quote)
						{
							es_write(L"\"");
						}

						// right fill
						if (!is_right_aligned)
						{
							if (tot_column_width + column_width > (tot_column_text_len + column_text_len))
							{
								fill = tot_column_width + column_width - (tot_column_text_len + column_text_len);

								// dont right fill last column.
								if (columni != es_numcolumns - 1)
								{
									// right spaces.
									es_fill(fill,' ');
									tot_column_text_len += fill;
								}
							}
						}
							
									
						tot_column_text_len += column_text_len;
						tot_column_width += es_column_widths[es_columns[columni]];
					}

					// restore color
					if (did_set_color)
					{
						es_cibuf_attributes = es_default_attributes;
						SetConsoleTextAttribute(es_output_handle,es_default_attributes);
					}
				}
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
				es_write(L"\r\n");
			}
		}

		es_cibuf_y++;
		count--;
		i++;
	}
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
	
		data = es_get_column_data(list,i,ES_COLUMN_SIZE);

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
	es_write(L"\r\n");
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

							es_cibuf = es_mem_alloc(es_console_wide * sizeof(CHAR_INFO));
							
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
								es_mem_free(es_cibuf);
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
	es_write(L"ES " ES_VERSION L"\r\n");
	es_write(L"ES is a command line interface to search Everything from a command prompt.\r\n");
	es_write(L"ES uses the Everything search syntax.\r\n");
	es_write(L"\r\n");
	es_write(L"Usage: es.exe [options] search text\r\n");
	es_write(L"Example: ES  Everything ext:exe;ini \r\n");
	es_write(L"\r\n");
	es_write(L"\r\n");
	es_write(L"Search options\r\n");
	es_write(L"   -r <search>, -regex <search>\r\n");
	es_write(L"        Search using regular expressions.\r\n");
	es_write(L"   -i, -case\r\n");
	es_write(L"        Match case.\r\n");
	es_write(L"   -w, -ww, -whole-word, -whole-words\r\n");
	es_write(L"        Match whole words.\r\n");
	es_write(L"   -p, -match-path\r\n");
	es_write(L"        Match full path and file name.\r\n");
	es_write(L"   -a, -diacritics\r\n");
	es_write(L"        Match diacritical marks.\r\n");
	es_write(L"\r\n");
	es_write(L"   -o <offset>, -offset <offset>\r\n");
	es_write(L"        Show results starting from offset.\r\n");
	es_write(L"   -n <num>, -max-results <num>\r\n");
	es_write(L"        Limit the number of results shown to <num>.\r\n");
	es_write(L"\r\n");
	es_write(L"   -path <path>\r\n");
	es_write(L"        Search for subfolders and files in path.\r\n");
	es_write(L"   -parent-path <path>\r\n");
	es_write(L"        Search for subfolders and files in the parent of path.\r\n");
	es_write(L"   -parent <path>\r\n");
	es_write(L"        Search for files with the specified parent path.\r\n");
	es_write(L"\r\n");
	es_write(L"   /ad\r\n");
	es_write(L"        Folders only.\r\n");
	es_write(L"   /a-d\r\n");
	es_write(L"        Files only.\r\n");
	es_write(L"   /a[RHSDAVNTPLCOIE]\r\n");
	es_write(L"        DIR style attributes search.\r\n");
	es_write(L"        R = Read only.\r\n");
	es_write(L"        H = Hidden.\r\n");
	es_write(L"        S = System.\r\n");
	es_write(L"        D = Directory.\r\n");
	es_write(L"        A = Archive.\r\n");
	es_write(L"        V = Device.\r\n");
	es_write(L"        N = Normal.\r\n");
	es_write(L"        T = Temporary.\r\n");
	es_write(L"        P = Sparse file.\r\n");
	es_write(L"        L = Reparse point.\r\n");
	es_write(L"        C = Compressed.\r\n");
	es_write(L"        O = Offline.\r\n");
	es_write(L"        I = Not content indexed.\r\n");
	es_write(L"        E = Encrypted.\r\n");
	es_write(L"        - = Prefix a flag with - to exclude.\r\n");
	es_write(L"\r\n");
	es_write(L"\r\n");
	es_write(L"Sort options\r\n");
	es_write(L"   -s\r\n");
	es_write(L"        sort by full path.\r\n");
	es_write(L"   -sort <name[-ascending|-descending]>, -sort-<name>[-ascending|-descending]\r\n");
	es_write(L"        Set sort\r\n");
	es_write(L"        name=name|path|size|extension|date-created|date-modified|date-accessed|\r\n");
	es_write(L"        attributes|file-list-file-name|run-count|date-recently-changed|date-run\r\n");
	es_write(L"   -sort-ascending, -sort-descending\r\n");
	es_write(L"        Set sort order\r\n");
	es_write(L"\r\n");
	es_write(L"   /on, /o-n, /os, /o-s, /oe, /o-e, /od, /o-d\r\n");
	es_write(L"        DIR style sorts.\r\n");
	es_write(L"        N = Name.\r\n");
	es_write(L"        S = Size.\r\n");
	es_write(L"        E = Extension.\r\n");
	es_write(L"        D = Date modified.\r\n");
	es_write(L"        - = Sort in descending order.\r\n");
	es_write(L"\r\n");
	es_write(L"\r\n");
	es_write(L"Display options\r\n");
	es_write(L"   -name\r\n");
	es_write(L"   -path-column\r\n");
	es_write(L"   -full-path-and-name, -filename-column\r\n");
	es_write(L"   -extension, -ext\r\n");
	es_write(L"   -size\r\n");
	es_write(L"   -date-created, -dc\r\n");
	es_write(L"   -date-modified, -dm\r\n");
	es_write(L"   -date-accessed, -da\r\n");
	es_write(L"   -attributes, -attribs, -attrib\r\n");
	es_write(L"   -file-list-file-name\r\n");
	es_write(L"   -run-count\r\n");
	es_write(L"   -date-run\r\n");
	es_write(L"   -date-recently-changed, -rc\r\n");
	es_write(L"        Show the specified column.\r\n");
	es_write(L"        \r\n");
	es_write(L"   -highlight\r\n");
	es_write(L"        Highlight results.\r\n");
	es_write(L"   -highlight-color <color>\r\n");
	es_write(L"        Highlight color 0-255.\r\n");
	es_write(L"\r\n");
	es_write(L"   -csv\r\n");
	es_write(L"   -efu\r\n");
	es_write(L"   -txt\r\n");
	es_write(L"   -m3u\r\n");
	es_write(L"   -m3u8\r\n");
	es_write(L"   -tsv\r\n");
	es_write(L"        Change display format.\r\n");
	es_write(L"\r\n");
	es_write(L"   -size-format <format>\r\n");
	es_write(L"        0=auto, 1=Bytes, 2=KB, 3=MB.\r\n");
	es_write(L"   -date-format <format>\r\n");
	es_write(L"        0=auto, 1=ISO-8601, 2=FILETIME, 3=ISO-8601(UTC)\r\n");
	es_write(L"\r\n");
	es_write(L"   -filename-color <color>\r\n");
	es_write(L"   -name-color <color>\r\n");
	es_write(L"   -path-color <color>\r\n");
	es_write(L"   -extension-color <color>\r\n");
	es_write(L"   -size-color <color>\r\n");
	es_write(L"   -date-created-color <color>, -dc-color <color>\r\n");
	es_write(L"   -date-modified-color <color>, -dm-color <color>\r\n");
	es_write(L"   -date-accessed-color <color>, -da-color <color>\r\n");
	es_write(L"   -attributes-color <color>\r\n");
	es_write(L"   -file-list-filename-color <color>\r\n");
	es_write(L"   -run-count-color <color>\r\n");
	es_write(L"   -date-run-color <color>\r\n");
	es_write(L"   -date-recently-changed-color <color>, -rc-color <color>\r\n");
	es_write(L"        Set the column color 0-255.\r\n");
	es_write(L"\r\n");
	es_write(L"   -filename-width <width>\r\n");
	es_write(L"   -name-width <width>\r\n");
	es_write(L"   -path-width <width>\r\n");
	es_write(L"   -extension-width <width>\r\n");
	es_write(L"   -size-width <width>\r\n");
	es_write(L"   -date-created-width <width>, -dc-width <width>\r\n");
	es_write(L"   -date-modified-width <width>, -dm-width <width>\r\n");
	es_write(L"   -date-accessed-width <width>, -da-width <width>\r\n");
	es_write(L"   -attributes-width <width>\r\n");
	es_write(L"   -file-list-filename-width <width>\r\n");
	es_write(L"   -run-count-width <width>\r\n");
	es_write(L"   -date-run-width <width>\r\n");
	es_write(L"   -date-recently-changed-width <width>, -rc-width <width>\r\n");
	es_write(L"        Set the column width 0-200.\r\n");
	es_write(L"\r\n");
	es_write(L"   -no-digit-grouping\r\n");
	es_write(L"        Don't group numbers with commas.\r\n");
	es_write(L"   -size-leading-zero\r\n");
	es_write(L"   -run-count-leading-zero\r\n");
	es_write(L"        Format the number with leading zeros, use with -no-digit-grouping.\r\n");
	es_write(L"   -double-quote\r\n");
	es_write(L"        Wrap paths and filenames with double quotes.\r\n");
	es_write(L"\r\n");
	es_write(L"\r\n");
	es_write(L"Export options\r\n");
	es_write(L"   -export-csv <out.csv>\r\n");
	es_write(L"   -export-efu <out.efu>\r\n");
	es_write(L"   -export-txt <out.txt>\r\n");
	es_write(L"   -export-m3u <out.m3u>\r\n");
	es_write(L"   -export-m3u8 <out.m3u8>\r\n");
	es_write(L"   -export-tsv <out.txt>\r\n");
	es_write(L"        Export to a file using the specified layout.\r\n");
	es_write(L"   -no-header\r\n");
	es_write(L"        Do not output a column header for CSV, EFU and TSV files.\r\n");
	es_write(L"   -utf8-bom\r\n");
	es_write(L"        Store a UTF-8 byte order mark at the start of the exported file.\r\n");
	es_write(L"\r\n");
	es_write(L"\r\n");
	es_write(L"General options\r\n");
	es_write(L"   -h, -help\r\n");
	es_write(L"        Display this help.\r\n");
	es_write(L"\r\n");
	es_write(L"   -instance <name>\r\n");
	es_write(L"        Connect to the unique Everything instance name.\r\n");
	es_write(L"   -ipc1, -ipc2\r\n");
	es_write(L"        Use IPC version 1 or 2.\r\n");
	es_write(L"   -pause, -more\r\n");
	es_write(L"        Pause after each page of output.\r\n");
	es_write(L"   -hide-empty-search-results\r\n");
	es_write(L"        Don't show any results when there is no search.\r\n");
	es_write(L"   -empty-search-help\r\n");
	es_write(L"        Show help when no search is specified.\r\n");
	es_write(L"   -timeout <milliseconds>\r\n");
	es_write(L"        Timeout after the specified number of milliseconds to wait for\r\n");
	es_write(L"        the Everything database to load before sending a query.\r\n");
	es_write(L"        \r\n");
	es_write(L"   -set-run-count <filename> <count>\r\n");
	es_write(L"        Set the run count for the specified filename.\r\n");
	es_write(L"   -inc-run-count <filename>\r\n");
	es_write(L"        Increment the run count for the specified filename by one.\r\n");
	es_write(L"   -get-run-count <filename>\r\n");
	es_write(L"        Display the run count for the specified filename.\r\n");
	es_write(L"   -get-result-count\r\n");
	es_write(L"        Display the result count for the specified search.\r\n");
	es_write(L"   -get-total-size\r\n");
	es_write(L"        Display the total result size for the specified search.\r\n");
	es_write(L"   -save-settings, -clear-settings\r\n");
	es_write(L"        Save or clear settings.\r\n");
	es_write(L"   -version\r\n");
	es_write(L"        Display ES major.minor.revision.build version and exit.\r\n");
	es_write(L"   -get-everything-version\r\n");
	es_write(L"        Display Everything major.minor.revision.build version and exit.\r\n");
	es_write(L"   -exit\r\n");
	es_write(L"        Exit Everything.\r\n");
	es_write(L"        Returns after Everything process closes.\r\n");
	es_write(L"   -save-db\r\n");
	es_write(L"        Save the Everything database to disk.\r\n");
	es_write(L"        Returns after saving completes.\r\n");
	es_write(L"   -reindex\r\n");
	es_write(L"        Force Everything to reindex.\r\n");
	es_write(L"        Returns after indexing completes.\r\n");
	es_write(L"   -no-result-error\r\n");
	es_write(L"        Set the error level if no results are found.\r\n");
	es_write(L"\r\n");
	es_write(L"\r\n");
	es_write(L"Notes \r\n");
	es_write(L"    Internal -'s in options can be omitted, eg: -nodigitgrouping\r\n");
	es_write(L"    Switches can also start with a /\r\n");
	es_write(L"    Use double quotes to escape spaces and switches.\r\n");
	es_write(L"    Switches can be disabled by prefixing them with no-, eg: -no-size.\r\n");
	es_write(L"    Use a ^ prefix or wrap with double quotes (\") to escape \\ & | > < ^\r\n");
}

// main entry
int main(int argc,char **argv)
{
	WNDCLASSEX wcex;
	HWND hwnd;
	MSG msg;
	int ret;
	wchar_t *d;
	wchar_t *e;
	const wchar_t *command_line;
	int perform_search;
	
	perform_search = 1;
	
	es_zero_memory(&wcex,sizeof(wcex));
	wcex.cbSize = sizeof(wcex);
	
	es_output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	
	es_cp = GetConsoleCP();

	// get console info.
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		DWORD mode;

		if (GetConsoleMode(es_output_handle,&mode))
		{
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
				es_output_is_file = 0;
			}
		}
	}
			
	if (!GetClassInfoEx(GetModuleHandle(0),TEXT("IPCTEST"),&wcex))
	{
		es_zero_memory(&wcex,sizeof(wcex));
		wcex.cbSize = sizeof(wcex);
		wcex.hInstance = GetModuleHandle(0);
		wcex.lpfnWndProc = es_window_proc;
		wcex.lpszClassName = TEXT("IPCTEST");
		
		if (!RegisterClassEx(&wcex))
		{
			es_fatal(ES_ERROR_REGISTER_WINDOW_CLASS);
		}
	}
	
	hwnd = CreateWindow(TEXT("IPCTEST"),TEXT(""),0,0,0,0,0,0,0,GetModuleHandle(0),0);
	if (!hwnd)
	{
		es_fatal(ES_ERROR_CREATE_WINDOW);
	}

	// allow the everything window to send a reply.
	es_user32_hdll = LoadLibraryA("user32.dll");
	
	if (es_user32_hdll)
	{
		es_pChangeWindowMessageFilterEx = (BOOL (WINAPI *)(HWND hWnd,UINT message,DWORD action,PCHANGEFILTERSTRUCT pChangeFilterStruct))GetProcAddress(es_user32_hdll,"ChangeWindowMessageFilterEx");

		if (es_pChangeWindowMessageFilterEx)
		{
			es_pChangeWindowMessageFilterEx(hwnd,WM_COPYDATA,MSGFLT_ALLOW,0);
		}
	}
	
	es_instance = es_mem_alloc(ES_WSTRING_SIZE * sizeof(wchar_t));
	es_search = es_mem_alloc(ES_SEARCH_BUF_SIZE * sizeof(wchar_t));
	es_filter = es_mem_alloc(ES_FILTER_BUF_SIZE * sizeof(wchar_t));
	
	*es_instance = 0;
	*es_filter = 0;

	d = es_search;

	// allow room for null terminator
	e = es_search + ES_SEARCH_BUF_SIZE - 1;

	// load default settings.	
	es_load_settings();
	
	command_line = GetCommandLine();
	
/*
	// code page test
	
	printf("CP %u\n",GetConsoleCP());
	printf("CP output %u\n",GetConsoleOutputCP());
	
	MessageBox(0,command_line,L"command line",MB_OK);
	return 0;
//	printf("command line %S\n",command_line);
*/
	// expect the executable name in the first argv.
	if (command_line)
	{
		command_line = es_get_argv(command_line);
	}
	
	if (command_line)
	{
		command_line = es_get_argv(command_line);
		
		if (command_line)
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
				if (es_check_param(es_argv,L"set-run-count"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_run_history_size = sizeof(EVERYTHING_IPC_RUN_HISTORY) + ((es_wstring_length(es_argv) + 1) * sizeof(wchar_t));
					es_run_history_data = es_mem_alloc(es_run_history_size);
					es_wstring_copy((wchar_t *)(((EVERYTHING_IPC_RUN_HISTORY *)es_run_history_data)+1),es_argv);
					
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					((EVERYTHING_IPC_RUN_HISTORY *)es_run_history_data)->run_count = es_wstring_to_int(es_argv);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_SET_RUN_COUNTW;
				}
				else			
				if (es_check_param(es_argv,L"inc-run-count"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_run_history_size = (es_wstring_length(es_argv) + 1) * sizeof(wchar_t);
					es_run_history_data = es_wstring_alloc(es_argv);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_INC_RUN_COUNTW;
				}
				else			
				if (es_check_param(es_argv,L"get-run-count"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_run_history_size = (es_wstring_length(es_argv) + 1) * sizeof(wchar_t);
					es_run_history_data = es_wstring_alloc(es_argv);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_GET_RUN_COUNTW;
				}
				else
				if ((es_check_param(es_argv,L"r")) || (es_check_param(es_argv,L"regex")))
				{
					const wchar_t *s;
					
					command_line = es_get_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
							
					if ((d != es_search) && (d < e)) *d++ = ' ';

					s = L"regex:";
					while(*s)
					{
						if (d < e) *d++ = *s;
						s++;
					}
										
					s = es_argv;
					while(*s)
					{
						if (d < e) *d++ = *s;
						s++;
					}		
				}
				else
				if ((es_check_param(es_argv,L"i")) || (es_check_param(es_argv,L"case")))
				{
					es_match_case = 1;
				}
				else
				if ((es_check_param(es_argv,L"no-i")) || (es_check_param(es_argv,L"no-case")))
				{
					es_match_case = 0;
				}
				else
				if ((es_check_param(es_argv,L"a")) || (es_check_param(es_argv,L"diacritics")))
				{
					es_match_diacritics = 1;
				}
				else
				if ((es_check_param(es_argv,L"no-a")) || (es_check_param(es_argv,L"no-diacritics")))
				{
					es_match_diacritics = 0;
				}
				else
				if (es_check_param(es_argv,L"instance"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_wstring_copy(es_instance,es_argv);
				}
				else
				if ((es_check_param(es_argv,L"exit")) || (es_check_param(es_argv,L"quit")))
				{
					es_exit_everything = 1;
				}
				else
				if ((es_check_param(es_argv,L"re-index")) || (es_check_param(es_argv,L"re-build")) || (es_check_param(es_argv,L"update")))
				{
					es_reindex = 1;
				}
				else
				if (es_check_param(es_argv,L"save-db"))
				{
					es_save_db = 1;
				}
				else
				if (es_check_param(es_argv,L"connect"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					if (es_connect)
					{
						es_mem_free(es_connect);
					}
					
					es_connect = es_wstring_alloc(es_argv);
				}
				else
				if (es_check_param(es_argv,L"highlight-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_highlight_color = es_wstring_to_int(es_argv);
				}
				else
				if (es_check_param(es_argv,L"highlight"))
				{
					es_highlight = 1;
				}
				else			
				if (es_check_param(es_argv,L"no-highlight"))
				{
					es_highlight = 0;
				}
				else
				if (es_check_param(es_argv,L"m3u"))
				{
					es_export = ES_EXPORT_M3U;
				}
				else				
				if (es_check_param(es_argv,L"export-m3u"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_export_file = CreateFile(es_argv,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export = ES_EXPORT_M3U;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else					
				if (es_check_param(es_argv,L"m3u8"))
				{
					es_export = ES_EXPORT_M3U8;
				}
				else				
				if (es_check_param(es_argv,L"export-m3u8"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_export_file = CreateFile(es_argv,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export = ES_EXPORT_M3U8;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (es_check_param(es_argv,L"csv"))
				{
					es_export = ES_EXPORT_CSV;
				}
				else				
				if (es_check_param(es_argv,L"tsv"))
				{
					es_export = ES_EXPORT_TSV;
				}
				else				
				if (es_check_param(es_argv,L"export-csv"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_export_file = CreateFile(es_argv,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export = ES_EXPORT_CSV;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (es_check_param(es_argv,L"export-tsv"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_export_file = CreateFile(es_argv,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export = ES_EXPORT_TSV;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (es_check_param(es_argv,L"efu"))
				{
					es_export = ES_EXPORT_EFU;
				}
				else		
				if (es_check_param(es_argv,L"export-efu"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_export_file = CreateFile(es_argv,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export = ES_EXPORT_EFU;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (es_check_param(es_argv,L"txt"))
				{
					es_export = ES_EXPORT_TXT;
				}
				else				
				if (es_check_param(es_argv,L"export-txt"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_export_file = CreateFile(es_argv,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export = ES_EXPORT_TXT;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else
				if (es_check_param(es_argv,L"cp"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_cp = es_wstring_to_int(es_argv);
				}
				else
				if ((es_check_param(es_argv,L"w")) || (es_check_param(es_argv,L"ww")) || (es_check_param(es_argv,L"whole-word")) || (es_check_param(es_argv,L"whole-words")))
				{
					es_match_whole_word = 1;
				}
				else
				if ((es_check_param(es_argv,L"no-w")) || (es_check_param(es_argv,L"no-ww")) || (es_check_param(es_argv,L"no-whole-word")) || (es_check_param(es_argv,L"no-whole-words")))
				{
					es_match_whole_word = 0;
				}
				else
				if ((es_check_param(es_argv,L"p")) || (es_check_param(es_argv,L"match-path")))
				{
					es_match_path = 1;
				}
				else
				if (es_check_param(es_argv,L"no-p"))
				{
					es_match_path = 0;
				}
				else
				if ((es_check_param(es_argv,L"file-name-width")) || (es_check_param(es_argv,L"file-name-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_FILENAME);
					es_column_widths[ES_COLUMN_HIGHLIGHTED_FILENAME] = es_column_widths[ES_COLUMN_FILENAME];
				}
				else
				if ((es_check_param(es_argv,L"name-width")) || (es_check_param(es_argv,L"name-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_NAME);
					es_column_widths[ES_COLUMN_HIGHLIGHTED_NAME] = es_column_widths[ES_COLUMN_NAME];
				}
				else
				if ((es_check_param(es_argv,L"path-width")) || (es_check_param(es_argv,L"path-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_PATH);
					es_column_widths[ES_COLUMN_HIGHLIGHTED_PATH] = es_column_widths[ES_COLUMN_PATH];
				}
				else
				if ((es_check_param(es_argv,L"extension-width")) || (es_check_param(es_argv,L"extension-wide")) || (es_check_param(es_argv,L"ext-width")) || (es_check_param(es_argv,L"ext-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_EXTENSION);
				}
				else
				if ((es_check_param(es_argv,L"size-width")) || (es_check_param(es_argv,L"size-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_SIZE);
				}
				else
				if ((es_check_param(es_argv,L"date-created-width")) || (es_check_param(es_argv,L"date-created-wide")) || (es_check_param(es_argv,L"dc-width")) || (es_check_param(es_argv,L"dc-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_DATE_CREATED);
				}
				else
				if ((es_check_param(es_argv,L"date-modified-width")) || (es_check_param(es_argv,L"date-modified-wide")) || (es_check_param(es_argv,L"dm-width")) || (es_check_param(es_argv,L"dm-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_DATE_MODIFIED);
				}
				else
				if ((es_check_param(es_argv,L"date-accessed-width")) || (es_check_param(es_argv,L"date-accessed-wide")) || (es_check_param(es_argv,L"da-width")) || (es_check_param(es_argv,L"da-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_DATE_ACCESSED);
				}
				else
				if ((es_check_param(es_argv,L"attributes-width")) || (es_check_param(es_argv,L"attributes-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_ATTRIBUTES);
				}
				else
				if ((es_check_param(es_argv,L"file-list-file-name-width")) || (es_check_param(es_argv,L"file-list-file-name-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_FILE_LIST_FILENAME);
				}
				else
				if ((es_check_param(es_argv,L"run-count-width")) || (es_check_param(es_argv,L"run-count-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_RUN_COUNT);
				}
				else
				if ((es_check_param(es_argv,L"date-run-width")) || (es_check_param(es_argv,L"date-run-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_DATE_RUN);
				}
				else
				if ((es_check_param(es_argv,L"date-recently-changed-width")) || (es_check_param(es_argv,L"date-recently-changed-wide")) || (es_check_param(es_argv,L"rc-width")) || (es_check_param(es_argv,L"rc-wide")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_column_wide(ES_COLUMN_DATE_RECENTLY_CHANGED);
				}
				else
				if (es_check_param(es_argv,L"size-leading-zero"))
				{
					es_size_leading_zero = 1;
				}
				else
				if (es_check_param(es_argv,L"no-size-leading-zero"))
				{
					es_size_leading_zero = 0;
				}
				else
				if (es_check_param(es_argv,L"run-count-leading-zero"))
				{
					es_run_count_leading_zero = 1;
				}
				else
				if (es_check_param(es_argv,L"no-run-count-leading-zero"))
				{
					es_run_count_leading_zero = 0;
				}
				else
				if (es_check_param(es_argv,L"no-digit-grouping"))
				{
					es_digit_grouping = 0;
				}
				else
				if (es_check_param(es_argv,L"digit-grouping"))
				{
					es_digit_grouping = 1;
				}
				else
				if (es_check_param(es_argv,L"size-format"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_size_format = es_wstring_to_int(es_argv);
				}
				else
				if (es_check_param(es_argv,L"date-format"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_date_format = es_wstring_to_int(es_argv);
				}
				else			
				if ((es_check_param(es_argv,L"pause")) || (es_check_param(es_argv,L"more")))
				{
					es_pause = 1;
				}
				else
				if ((es_check_param(es_argv,L"no-pause")) || (es_check_param(es_argv,L"no-more")))
				{
					es_pause = 0;
				}
				else
				if (es_check_param(es_argv,L"empty-search-help"))
				{
					es_empty_search_help = 1;
					es_hide_empty_search_results = 0;
				}
				else
				if (es_check_param(es_argv,L"no-empty-search-help"))
				{
					es_empty_search_help = 0;
				}
				else
				if (es_check_param(es_argv,L"hide-empty-search-results"))
				{
					es_hide_empty_search_results = 1;
					es_empty_search_help = 0;
				}
				else
				if (es_check_param(es_argv,L"no-hide-empty-search-results"))
				{
					es_hide_empty_search_results = 0;
				}
				else
				if (es_check_param(es_argv,L"save-settings"))
				{
					es_save = 1;
				}
				else
				if (es_check_param(es_argv,L"clear-settings"))
				{
					char filename[MAX_PATH];
					
					if (es_get_ini_filename(filename))
					{
						DeleteFileA(filename);

						es_write(L"Settings saved.\r\n");
					}
					
					goto exit;
				}
				else
				if (es_check_param(es_argv,L"filename-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_set_color(ES_COLUMN_FILENAME);
					es_column_color[ES_COLUMN_HIGHLIGHTED_FILENAME] = es_column_color[ES_COLUMN_FILENAME];
					es_column_color_is_valid[ES_COLUMN_HIGHLIGHTED_FILENAME] = es_column_color_is_valid[ES_COLUMN_FILENAME];
				}
				else			
				if (es_check_param(es_argv,L"name-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_NAME);
					es_column_color[ES_COLUMN_NAME] = es_column_color[ES_COLUMN_NAME];
					es_column_color_is_valid[ES_COLUMN_NAME] = es_column_color_is_valid[ES_COLUMN_NAME];
				}
				else			
				if (es_check_param(es_argv,L"path-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_PATH);
					es_column_color[ES_COLUMN_PATH] = es_column_color[ES_COLUMN_PATH];
					es_column_color_is_valid[ES_COLUMN_PATH] = es_column_color_is_valid[ES_COLUMN_PATH];
				}
				else		
				if (es_check_param(es_argv,L"extension-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_EXTENSION);
				}
				else
				if (es_check_param(es_argv,L"size-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_SIZE);
				}
				else
				if ((es_check_param(es_argv,L"date-created-color")) || (es_check_param(es_argv,L"dc-color")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_DATE_CREATED);
				}
				else
				if ((es_check_param(es_argv,L"date-modified-color")) || (es_check_param(es_argv,L"dm-color")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_DATE_MODIFIED);
				}
				else
				if ((es_check_param(es_argv,L"date-accessed-color")) || (es_check_param(es_argv,L"da-color")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_DATE_ACCESSED);
				}
				else			
				if ((es_check_param(es_argv,L"attributes-color")) || (es_check_param(es_argv,L"attribs-color")) || (es_check_param(es_argv,L"attrib-color"))) 
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_ATTRIBUTES);
				}
				else			
				if (es_check_param(es_argv,L"file-list-filename-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_FILE_LIST_FILENAME);
				}
				else
				if (es_check_param(es_argv,L"run-count-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_RUN_COUNT);
				}
				else			
				if (es_check_param(es_argv,L"date-run-color"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_DATE_RUN);
				}
				else
				if ((es_check_param(es_argv,L"date-recently-changed-color")) || (es_check_param(es_argv,L"rc-color")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
								
					es_set_color(ES_COLUMN_DATE_RECENTLY_CHANGED);
				}
				else
				if (es_check_param(es_argv,L"name"))
				{
					es_add_column(ES_COLUMN_NAME);
				}
				else
				if (es_check_param(es_argv,L"no-name"))
				{
					es_remove_column(ES_COLUMN_NAME);
				}
				else
				if (es_check_param(es_argv,L"path-column"))
				{
					es_add_column(ES_COLUMN_PATH);
				}
				else
				if (es_check_param(es_argv,L"no-path-column"))
				{
					es_remove_column(ES_COLUMN_PATH);
				}
				else
				if ((es_check_param(es_argv,L"full-path-and-name")) || (es_check_param(es_argv,L"path-and-name")) || (es_check_param(es_argv,L"filename-column")))
				{
					es_add_column(ES_COLUMN_FILENAME);
				}
				else
				if ((es_check_param(es_argv,L"no-full-path-and-name")) || (es_check_param(es_argv,L"no-path-and-name")) || (es_check_param(es_argv,L"no-filename-column")))
				{
					es_remove_column(ES_COLUMN_FILENAME);
				}
				else
				if ((es_check_param(es_argv,L"extension")) || (es_check_param(es_argv,L"ext")))
				{
					es_add_column(ES_COLUMN_EXTENSION);
				}
				else
				if ((es_check_param(es_argv,L"no-extension")) || (es_check_param(es_argv,L"no-ext")))
				{
					es_remove_column(ES_COLUMN_EXTENSION);
				}
				else
				if (es_check_param(es_argv,L"size"))
				{
					es_add_column(ES_COLUMN_SIZE);
				}
				else
				if (es_check_param(es_argv,L"no-size"))
				{
					es_remove_column(ES_COLUMN_SIZE);
				}
				else
				if ((es_check_param(es_argv,L"date-created")) || (es_check_param(es_argv,L"dc")))
				{
					es_add_column(ES_COLUMN_DATE_CREATED);
				}
				else
				if ((es_check_param(es_argv,L"no-date-created")) || (es_check_param(es_argv,L"no-dc")))
				{
					es_remove_column(ES_COLUMN_DATE_CREATED);
				}
				else
				if ((es_check_param(es_argv,L"date-modified")) || (es_check_param(es_argv,L"dm")))
				{
					es_add_column(ES_COLUMN_DATE_MODIFIED);
				}
				else
				if ((es_check_param(es_argv,L"no-date-modified")) || (es_check_param(es_argv,L"no-dm")))
				{
					es_remove_column(ES_COLUMN_DATE_MODIFIED);
				}
				else
				if ((es_check_param(es_argv,L"date-accessed")) || (es_check_param(es_argv,L"da")))
				{
					es_add_column(ES_COLUMN_DATE_ACCESSED);
				}
				else
				if ((es_check_param(es_argv,L"no-date-accessed")) || (es_check_param(es_argv,L"no-da")))
				{
					es_remove_column(ES_COLUMN_DATE_ACCESSED);
				}
				else
				if ((es_check_param(es_argv,L"attributes")) || (es_check_param(es_argv,L"attribs")) || (es_check_param(es_argv,L"attrib")))
				{
					es_add_column(ES_COLUMN_ATTRIBUTES);
				}
				else
				if ((es_check_param(es_argv,L"no-attributes")) || (es_check_param(es_argv,L"no-attribs")) || (es_check_param(es_argv,L"no-attrib")))
				{
					es_remove_column(ES_COLUMN_ATTRIBUTES);
				}
				else
				if ((es_check_param(es_argv,L"file-list-file-name")) || (es_check_param(es_argv,L"flfn")))
				{
					es_add_column(ES_COLUMN_FILE_LIST_FILENAME);
				}
				else
				if ((es_check_param(es_argv,L"no-file-list-file-name")) || (es_check_param(es_argv,L"no-flfn")))
				{
					es_remove_column(ES_COLUMN_FILE_LIST_FILENAME);
				}
				else
				if (es_check_param(es_argv,L"run-count"))
				{
					es_add_column(ES_COLUMN_RUN_COUNT);
				}
				else
				if (es_check_param(es_argv,L"no-run-count"))
				{
					es_remove_column(ES_COLUMN_RUN_COUNT);
				}
				else
				if (es_check_param(es_argv,L"date-run"))
				{
					es_add_column(ES_COLUMN_DATE_RUN);
				}
				else
				if (es_check_param(es_argv,L"no-date-run"))
				{
					es_remove_column(ES_COLUMN_DATE_RUN);
				}
				else
				if ((es_check_param(es_argv,L"date-recently-changed")) || (es_check_param(es_argv,L"rc")) || (es_check_param(es_argv,L"drc")) || (es_check_param(es_argv,L"recent-change")))
				{
					es_add_column(ES_COLUMN_DATE_RECENTLY_CHANGED);
				}
				else
				if ((es_check_param(es_argv,L"no-date-recently-changed")) || (es_check_param(es_argv,L"no-rc")) || (es_check_param(es_argv,L"no-drc")) || (es_check_param(es_argv,L"no-recent-change")))
				{
					es_remove_column(ES_COLUMN_DATE_RECENTLY_CHANGED);
				}
				else
				if (es_check_param(es_argv,L"sort-ascending"))
				{
					es_sort_ascending = 1;
				}
				else
				if (es_check_param(es_argv,L"sort-descending"))
				{
					es_sort_ascending = -1;
				}
				else
				if (es_check_param(es_argv,L"sort"))
				{	
					wchar_t sortnamebuf[ES_WSTRING_SIZE];
					int sortnamei;
					
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					for(sortnamei=0;sortnamei<ES_SORT_NAME_COUNT;sortnamei++)
					{
						if (es_check_param_param(es_argv,es_sort_names[sortnamei]))
						{
							es_sort = es_sort_names_to_ids[sortnamei];

							break;
						}
						else
						{
							es_wstring_copy(sortnamebuf,es_sort_names[sortnamei]);
							es_wstring_cat(sortnamebuf,L"-ascending");
							
							if (es_check_param_param(es_argv,sortnamebuf))
							{
								es_sort = es_sort_names_to_ids[sortnamei];
								es_sort_ascending = 1;

								break;
							}
							else
							{
								es_wstring_copy(sortnamebuf,es_sort_names[sortnamei]);
								es_wstring_cat(sortnamebuf,L"-descending");
								
								if (es_check_param_param(es_argv,sortnamebuf))
								{
									es_sort = es_sort_names_to_ids[sortnamei];
									es_sort_ascending = -1;

									break;
								}
							}
						}
					}
					
					if (sortnamei == ES_SORT_NAME_COUNT)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if (es_check_param(es_argv,L"ipc1"))
				{	
					es_ipc_version = 0x01; 
				}
				else
				if (es_check_param(es_argv,L"ipc2"))
				{	
					es_ipc_version = 0x02; 
				}
				else
				if (es_check_param(es_argv,L"ipc3"))
				{	
					es_ipc_version = 0x04; 
				}
				else
				if (es_check_param(es_argv,L"header"))
				{	
					es_header = 1; 
				}
				else
				if (es_check_param(es_argv,L"no-header"))
				{	
					es_header = 0; 
				}
				else
				if (es_check_param(es_argv,L"double-quote"))
				{	
					es_double_quote = 1; 
					es_csv_double_quote = 1;
				}
				else
				if (es_check_param(es_argv,L"no-double-quote"))
				{	
					es_double_quote = 0; 
					es_csv_double_quote = 0;
				}
				else
				if (es_check_param(es_argv,L"version"))
				{	
					es_write(ES_VERSION);
					es_write(L"\r\n");

					goto exit;
				}
				else
				if (es_check_param(es_argv,L"get-everything-version"))
				{
					es_get_everything_version = 1;
				}
				else
				if (es_check_param(es_argv,L"utf8-bom"))
				{
					es_utf8_bom = 1;
				}
				else
				if (es_check_sorts())
				{
				}
				else
				if (es_check_param(es_argv,L"on"))
				{	
					es_sort = EVERYTHING_IPC_SORT_NAME_ASCENDING;
					es_sort_ascending = 1;
				}
				else
				if (es_check_param(es_argv,L"o-n"))
				{	
					es_sort = EVERYTHING_IPC_SORT_NAME_DESCENDING;
					es_sort_ascending = -1;
				}
				else
				if (es_check_param(es_argv,L"os"))
				{	
					es_sort = EVERYTHING_IPC_SORT_SIZE_ASCENDING;
					es_sort_ascending = 1;
				}
				else
				if (es_check_param(es_argv,L"o-s"))
				{	
					es_sort = EVERYTHING_IPC_SORT_SIZE_DESCENDING;
					es_sort_ascending = -1;
				}
				else
				if (es_check_param(es_argv,L"oe"))
				{	
					es_sort = EVERYTHING_IPC_SORT_EXTENSION_ASCENDING;
					es_sort_ascending = 1;
				}
				else
				if (es_check_param(es_argv,L"o-e"))
				{	
					es_sort = EVERYTHING_IPC_SORT_EXTENSION_DESCENDING;
					es_sort_ascending = -1;
				}
				else
				if (es_check_param(es_argv,L"od"))
				{	
					es_sort = EVERYTHING_IPC_SORT_DATE_MODIFIED_ASCENDING;
					es_sort_ascending = 1;
				}
				else
				if (es_check_param(es_argv,L"o-d"))
				{	
					es_sort = EVERYTHING_IPC_SORT_DATE_MODIFIED_DESCENDING;
					es_sort_ascending = -1;
				}
				else
				if (es_check_param(es_argv,L"s"))
				{
					es_sort = EVERYTHING_IPC_SORT_PATH_ASCENDING;
				}
				else
				if ((es_check_param(es_argv,L"n")) || (es_check_param(es_argv,L"max-results")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_max_results = es_wstring_to_int(es_argv);
				}
				else
				if ((es_check_param(es_argv,L"o")) || (es_check_param(es_argv,L"offset")))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_offset = es_wstring_to_int(es_argv);
				}
				else
				if (es_check_param(es_argv,L"path"))
				{
					wchar_t pathbuf[ES_WSTRING_SIZE];
					wchar_t *namepart;
				
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					// relative path.
					GetFullPathName(es_argv,ES_WSTRING_SIZE,pathbuf,&namepart);
					
					if (*es_filter)
					{
						es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L" ");
					}

					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"\"");
					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,pathbuf);
					if ((*pathbuf) && (pathbuf[es_wstring_length(pathbuf) - 1] != '\\'))
					{
						es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"\\");
					}
					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"\"");
				}
				else
				if (es_check_param(es_argv,L"parent-path"))
				{
					wchar_t pathbuf[ES_WSTRING_SIZE];
					wchar_t *namepart;
				
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					// relative path.
					GetFullPathName(es_argv,ES_WSTRING_SIZE,pathbuf,&namepart);
					PathRemoveFileSpec(pathbuf);
					
					if (*es_filter)
					{
						es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L" ");
					}

					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"\"");
					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,pathbuf);
					if ((*pathbuf) && (pathbuf[es_wstring_length(pathbuf) - 1] != '\\'))
					{
						es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"\\");
					}
					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"\"");
				}
				else
				if (es_check_param(es_argv,L"parent"))
				{
					wchar_t pathbuf[ES_WSTRING_SIZE];
					wchar_t *namepart;
				
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					// relative path.
					GetFullPathName(es_argv,ES_WSTRING_SIZE,pathbuf,&namepart);
					
					if (*es_filter)
					{
						es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L" ");
					}

					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"parent:\"");
					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,pathbuf);
					if ((*pathbuf) && (pathbuf[es_wstring_length(pathbuf) - 1] != '\\'))
					{
						es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"\\");
					}
					es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L"\"");
				}
				else
				if (es_check_param(es_argv,L"time-out"))
				{
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
					
					es_timeout = (DWORD)es_wstring_to_int(es_argv);
				}
				else
				if (es_check_param(es_argv,L"no-time-out"))
				{
					es_timeout = 0;
				}
				else
				if (es_check_param(es_argv,L"ad"))
				{	
					// add folder:
					es_append_filter(L"folder:");
				}
				else
				if (es_check_param(es_argv,L"a-d"))
				{	
					// add folder:
					es_append_filter(L"file:");
				}
				else
				if (es_check_param(es_argv,L"get-result-count"))
				{
					es_get_result_count = 1;
				}
				else
				if (es_check_param(es_argv,L"get-total-size"))
				{
					es_get_total_size = 1;
				}
				else
				if (es_check_param(es_argv,L"no-result-error"))
				{
					es_no_result_error = 1;
				}
				else
				if ((es_check_param(es_argv,L"q")) || (es_check_param(es_argv,L"search")))
				{
					const wchar_t *s;
					
					// this removes quotes from the search string.
					command_line = es_get_command_argv(command_line);
					if (!command_line)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
							
					if ((d != es_search) && (d < e)) *d++ = ' ';

					s = es_argv;
					while(*s)
					{
						if (d < e) *d++ = *s;
						s++;
					}		
				}
				else
				if ((es_check_param(es_argv,L"q*")) || (es_check_param(es_argv,L"search*")))
				{
					const wchar_t *s;

					// eat the rest.
					// this would do the same as a stop parsing switches command line option
					// like 7zip --
					// or powershell --%
					// this doesn't remove quotes.
					command_line = es_skip_ws(command_line);
							
					if ((d != es_search) && (d < e)) *d++ = ' ';

					s = command_line;
					while(*s)
					{
						if (d < e) *d++ = *s;
						s++;
					}		
					
					break;
				}
				else
			/*	if (es_check_param(es_argv,L"%"))
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
						
						if ((d != es_search) && (d < e)) *d++ = ' ';

						// copy append to search
						s = es_argv;
						while(*s)
						{
							if (d < e) *d++ = *s;
							s++;
						}
					}
					
					break;
				}
				else
			*/	if (((es_argv[0] == '-') || (es_argv[0] == '/')) && (es_argv[1] == 'a') && (es_argv[2]))
				{
					const wchar_t *p;
					wchar_t attrib[ES_WSTRING_SIZE];
					wchar_t notattrib[ES_WSTRING_SIZE];
					wchar_t wch[2];

					p = es_argv + 2;
					
					attrib[0] = 0;
					notattrib[0] = 0;
					
					// TODO handle unknown a switches.
					while(*p)
					{
						int lower_p;
						
						lower_p = tolower(*p);
						
						if (lower_p == '-')
						{
							p++;
							
							lower_p = tolower(*p);
							
							wch[0] = lower_p;
							wch[1] = 0;
							
							es_wstring_cat(notattrib,wch);
						}
						else
						{
							wch[0] = lower_p;
							wch[1] = 0;
							
							es_wstring_cat(attrib,wch);
						}
												
						p++;						
					}
					
					// copy append to search
					if (*attrib)
					{
						const wchar_t *s;
						
						if ((d != es_search) && (d < e)) *d++ = ' ';

						s = L"attrib:";
						while(*s)
						{
							if (d < e) *d++ = *s;
							s++;
						}
						
						// copy append to search
						s = attrib;
						while(*s)
						{
							if (d < e) *d++ = *s;
							s++;
						}
					}

					// copy not append to search
					if (*notattrib)
					{
						const wchar_t *s;
						
						if ((d != es_search) && (d < e)) *d++ = ' ';
	
						s = L"!attrib:";
						while(*s)
						{
							if (d < e) *d++ = *s;
							s++;
						}
						
						// copy append to search
						s = notattrib;
						while(*s)
						{
							if (d < e) *d++ = *s;
							s++;
						}
					}
				}
				else
				if ((es_check_param(es_argv,L"?")) || (es_check_param(es_argv,L"help")) || (es_check_param(es_argv,L"h")))
				{
					// user requested help
					es_help();
					
					goto exit;
				}
				else
				if ((es_argv[0] == '-') && (!es_is_literal_switch(es_argv)))
				{
					// unknown command
					// allow /downloads to search for "\downloads" for now
					es_fatal(ES_ERROR_UNKNOWN_SWITCH);
				}
				else
				{
					const wchar_t *s;
					
					if ((d != es_search) && (d < e)) *d++ = ' ';

					// copy append to search
					s = es_argv;
					while(*s)
					{
						if (d < e) *d++ = *s;
						s++;
					}
				}

				command_line = es_get_argv(command_line);
				if (!command_line)
				{
					break;
				}
			}
		}
	}
	
	// fix sort order
	if (es_sort_ascending)
	{
		if (es_sort_ascending > 0)
		{
			es_set_sort_ascending();
		}
		else
		{
			es_set_sort_descending();
		}
	}
		
	// save settings.
	if (es_save)
	{
		es_save_settings();
		
		es_write(L"Settings saved.\r\n");
		
		perform_search = 0;
	}
	
	// save settings.
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
			es_write(L".");
			es_write_dword(minor);
			es_write(L".");
			es_write_dword(revision);
			es_write(L".");
			es_write_dword(build);
			es_write(L"\r\n");
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

			SendMessage(es_everything_hwnd,WM_CLOSE,0,0);

			// wait for Everything to exit.
			if (GetWindowThreadProcessId(es_everything_hwnd,&dwProcessId))
			{
				HANDLE h;
				
				h = OpenProcess(SYNCHRONIZE,FALSE,dwProcessId);
				if (h)
				{
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
		if ((d == es_search) && (!*es_filter) && (es_max_results == 0xffffffff) && (!es_get_result_count) && (!es_get_total_size))
		{
			if (es_empty_search_help)
			{
				es_help();
				
				goto exit;
			}

			if (es_hide_empty_search_results)
			{
				goto exit;
			}
		}
		
		// add filename column
		if (es_find_column(ES_COLUMN_FILENAME) == -1)
		{
			if (es_find_column(ES_COLUMN_NAME) == -1)
			{
				if (es_find_column(ES_COLUMN_PATH) == -1)
				{
					es_add_column(ES_COLUMN_FILENAME);
				}
			}
		}
		
		// apply highlighting to columns.
		if (es_highlight)
		{
			int columni;
			
			for(columni=0;columni<es_numcolumns;columni++)
			{
				if (es_columns[columni] == ES_COLUMN_FILENAME)
				{
					es_columns[columni] = ES_COLUMN_HIGHLIGHTED_FILENAME;
				}
				else
				if (es_columns[columni] == ES_COLUMN_PATH)
				{
					es_columns[columni] = ES_COLUMN_HIGHLIGHTED_PATH;
				}
				else
				if (es_columns[columni] == ES_COLUMN_NAME)
				{
					es_columns[columni] = ES_COLUMN_HIGHLIGHTED_NAME;
				}
			}
		}
		
		// null terminate
		*d = 0;

		// write export headers
		// write header
		if (es_export)
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
				int columni;
				
				for(columni=0;columni<es_numcolumns;columni++)
				{
					if (es_columns[columni] == ES_COLUMN_HIGHLIGHTED_FILENAME)
					{
						es_columns[columni] = ES_COLUMN_FILENAME;
					}
					else
					if (es_columns[columni] == ES_COLUMN_HIGHLIGHTED_PATH)
					{
						es_columns[columni] = ES_COLUMN_PATH;
					}
					else
					if (es_columns[columni] == ES_COLUMN_HIGHLIGHTED_NAME)
					{
						es_columns[columni] = ES_COLUMN_NAME;
					}
				}
			}
			
			if (es_export_file != INVALID_HANDLE_VALUE)
			{
				es_export_buf = es_mem_alloc(ES_EXPORT_BUF_SIZE);
				es_export_p = es_export_buf;
				es_export_remaining = ES_EXPORT_BUF_SIZE;
			}
			
			if ((es_export == ES_EXPORT_CSV) || (es_export == ES_EXPORT_TSV))
			{
				if (es_header)
				{
					int columni;
					
					for(columni=0;columni<es_numcolumns;columni++)
					{
						if (columni)
						{
							es_fwrite((es_export == ES_EXPORT_CSV) ? L"," : L"\t");
						}

						es_fwrite(es_column_names[es_columns[columni]]);
					}

					es_fwrite(L"\r\n");
				}
			}
			else
			if (es_export == ES_EXPORT_EFU)
			{
				int was_size_column;
				int was_date_modified_column;
				int was_date_created_column;
				int was_attributes_column;
				int column_i;

				was_size_column = 0;
				was_date_modified_column = 0;
				was_date_created_column = 0;
				was_attributes_column = 0;
							
				for(column_i=0;column_i<es_numcolumns;column_i++)
				{
					switch(es_columns[column_i])
					{
						case ES_COLUMN_SIZE:
							was_size_column = 1;
							break;
							
						case ES_COLUMN_DATE_MODIFIED:
							was_date_modified_column = 1;
							break;
							
						case ES_COLUMN_DATE_CREATED:
							was_date_created_column = 1;
							break;
							
						case ES_COLUMN_ATTRIBUTES:
							was_attributes_column = 1;
							break;
					}
				}
				
				// reset columns and force Filename,Size,Date Modified,Date Created,Attributes.
				es_numcolumns = 0;
				es_add_column(ES_COLUMN_FILENAME);
				
				if (was_size_column)
				{
					es_add_column(ES_COLUMN_SIZE);
				}

				if (was_date_modified_column)
				{
					es_add_column(ES_COLUMN_DATE_MODIFIED);
				}

				if (was_date_created_column)
				{
					es_add_column(ES_COLUMN_DATE_CREATED);
				}

				if (was_attributes_column)
				{
					es_add_column(ES_COLUMN_ATTRIBUTES);
				}

				if (es_header)
				{
					es_fwrite(L"Filename,Size,Date Modified,Date Created,Attributes\r\n");
				}
			}
			else
			if ((es_export == ES_EXPORT_TXT) || (es_export == ES_EXPORT_M3U) || (es_export == ES_EXPORT_M3U8))
			{
				// reset columns and force Filename.
				es_numcolumns = 0;
				es_add_column(ES_COLUMN_FILENAME);
			}
		}

		// fix search filter
		if (*es_filter)
		{
			wchar_t *new_search;
			
			new_search = es_mem_alloc(ES_SEARCH_BUF_SIZE * sizeof(wchar_t));
			
			*new_search = 0;
			
			es_wbuf_cat(new_search,ES_SEARCH_BUF_SIZE,L"< ");
			es_wbuf_cat(new_search,ES_SEARCH_BUF_SIZE,es_filter);
			es_wbuf_cat(new_search,ES_SEARCH_BUF_SIZE,L" > < ");
			es_wbuf_cat(new_search,ES_SEARCH_BUF_SIZE,es_search);
			if (_es_is_unbalanced_quotes(es_search))
			{
				// using a filter and a search without a trailing quote breaks the search
				// as it makes the trailing > literal.
				// add a terminating quote:
				es_wbuf_cat(new_search,ES_SEARCH_BUF_SIZE,L"\"");
			}
			es_wbuf_cat(new_search,ES_SEARCH_BUF_SIZE,L" >");
			
			es_mem_free(es_search);
			
			es_search = new_search;
		}

		{
			int got_indexed_file_info;
			
			got_indexed_file_info = 0;
			
			es_everything_hwnd = es_find_ipc_window();
			if (es_everything_hwnd)
			{
				if (!got_indexed_file_info)
				{
					if (es_export == ES_EXPORT_EFU)
					{
						// get indexed file info column for exporting.
						if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_FILE_SIZE))
						{
							es_add_column(ES_COLUMN_SIZE);
						}
						
						if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_DATE_MODIFIED))
						{
							es_add_column(ES_COLUMN_DATE_MODIFIED);
						}
						
						if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_DATE_CREATED))
						{
							es_add_column(ES_COLUMN_DATE_CREATED);
						}
						
						if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_ATTRIBUTES))
						{
							es_add_column(ES_COLUMN_ATTRIBUTES);
						}
					}
					
					got_indexed_file_info = 1;
				}
		
				if (es_ipc_version & 4)
				{
					if (es_sendquery3()) 
					{
						// success
						// don't try other versions.
						// we dont need a message loop, exit..
						goto exit;
					}
				}

				if (es_ipc_version & 2)
				{
					if (es_sendquery2(hwnd)) 
					{
						// success
						// don't try version 1.
						goto query_sent;
					}
				}

				if (es_ipc_version & 1)
				{
					if (es_sendquery(hwnd)) 
					{
						// success
						// don't try other versions.
						goto query_sent;
					}
				}

				es_fatal(ES_ERROR_SEND_MESSAGE);
			}
		}

query_sent:


		// message pump
	loop:

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
		
		goto loop;
	}

exit:

	if (es_run_history_data)
	{
		es_mem_free(es_run_history_data);
	}

	if (es_argv)
	{
		es_mem_free(es_argv);
	}

	if (es_search)
	{
		es_mem_free(es_search);
	}
	
	if (es_filter)
	{
		es_mem_free(es_filter);
	}
	
	if (es_instance)
	{
		es_mem_free(es_instance);
	}
	
	if (es_connect)
	{
		es_mem_free(es_connect);
	}

	es_flush();
	
	if (es_export_buf)
	{
		es_mem_free(es_export_buf);
	}
	
	if (es_export_file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(es_export_file);
	}
	
	if (es_user32_hdll)
	{
		FreeLibrary(es_user32_hdll);
	}
		
	if (es_ret != ES_ERROR_SUCCESS)
	{
		es_fatal(es_ret);
	}
		
	return ES_ERROR_SUCCESS;
}

int es_wstring_to_int(const wchar_t *s)
{
	const wchar_t *p;
	int value;
	
	p = s;
	value = 0;
	
	if ((*p == '0') && ((p[1] == 'x') || (p[1] == 'X')))
	{
		p += 2;
		
		while(*p)
		{
			if ((*p >= '0') && (*p <= '9'))
			{
				value *= 16;
				value += *p - '0';
			}
			else
			if ((*p >= 'A') && (*p <= 'F'))
			{
				value *= 16;
				value += *p - 'A' + 10;
			}
			else
			if ((*p >= 'a') && (*p <= 'f'))
			{
				value *= 16;
				value += *p - 'a' + 10;
			}
			else
			{
				break;
			}
			
			p++;
		}
	}
	else
	{
		while(*p)
		{
			if (!((*p >= '0') && (*p <= '9')))
			{
				break;
			}
			
			value *= 10;
			value += *p - '0';
			p++;
		}
	}
	
	return value;
}

// find the Everything IPC window
HWND es_find_ipc_window(void)
{
	DWORD tickstart;
	DWORD tick;
	wchar_t window_class[ES_WSTRING_SIZE];
	HWND ret;

	tickstart = GetTickCount();
	
	*window_class = 0;
	
	es_wstring_cat(window_class,EVERYTHING_IPC_WNDCLASS);
	
	if (*es_instance)
	{
		es_wstring_cat(window_class,L"_(");
		es_wstring_cat(window_class,es_instance);
		es_wstring_cat(window_class,L")");
	}

	ret = 0;
	
	for(;;)
	{
		HWND hwnd;
		
		hwnd = FindWindow(window_class,0);

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
	
	return ret;
}

// find the Everything IPC window
static void es_wait_for_db_loaded(void)
{
	DWORD tickstart;
	wchar_t window_class[ES_WSTRING_SIZE];

	tickstart = GetTickCount();
	
	*window_class = 0;
	
	es_wstring_cat(window_class,EVERYTHING_IPC_WNDCLASS);
	
	if (*es_instance)
	{
		es_wstring_cat(window_class,L"_(");
		es_wstring_cat(window_class,es_instance);
		es_wstring_cat(window_class,L")");
	}

	for(;;)
	{
		HWND hwnd;
		DWORD tick;
		
		hwnd = FindWindow(window_class,0);

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
}

// find the Everything IPC window
static void es_wait_for_db_not_busy(void)
{
	DWORD tickstart;
	wchar_t window_class[ES_WSTRING_SIZE];

	tickstart = GetTickCount();
	
	*window_class = 0;
	
	es_wstring_cat(window_class,EVERYTHING_IPC_WNDCLASS);
	
	if (*es_instance)
	{
		es_wstring_cat(window_class,L"_(");
		es_wstring_cat(window_class,es_instance);
		es_wstring_cat(window_class,L")");
	}

	for(;;)
	{
		HWND hwnd;
		DWORD tick;
		
		hwnd = FindWindow(window_class,0);

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
}

void es_wstring_copy(wchar_t *buf,const wchar_t *s)
{
	int max;
	
	max = ES_WSTRING_SIZE - 1;
	while(max)
	{
		if (!*s) 
		{
			break;
		}
		
		*buf = *s;
		
		buf++;
		s++;
		max--;
	}
	
	*buf = 0;
}

// cat a string to buf
// max MUST be > 0
void es_wstring_cat(wchar_t *buf,const wchar_t *s)
{
	es_wbuf_cat(buf,ES_WSTRING_SIZE,s);
}

// cat a string to buf
// max MUST be > 0
void es_wstring_cat_UINT64(wchar_t *buf,ES_UINT64 qw)
{
	wchar_t qwbuf[ES_WSTRING_SIZE];
	
	es_wstring_print_UINT64(qwbuf,qw);
	
	es_wstring_cat(buf,qwbuf);
	
}

int es_check_param(wchar_t *param,const wchar_t *s)
{
	if ((*param == '-') || (*param == '/'))
	{
		param++;
		
		return es_check_param_param(param,s);
	}
	
	return 0;
}

int es_check_param_param(wchar_t *param,const wchar_t *s)
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

void es_set_sort_ascending(void)
{
	switch(es_sort)
	{
		case EVERYTHING_IPC_SORT_NAME_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_NAME_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_PATH_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_PATH_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_SIZE_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_SIZE_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_EXTENSION_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_EXTENSION_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_TYPE_NAME_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_TYPE_NAME_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_CREATED_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_CREATED_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_MODIFIED_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_MODIFIED_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_ATTRIBUTES_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_ATTRIBUTES_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_RUN_COUNT_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_RUN_COUNT_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_ACCESSED_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_ACCESSED_ASCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_RUN_DESCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_RUN_ASCENDING;
			break;
	}
}
	
void es_set_sort_descending(void)
{
	switch(es_sort)
	{
		case EVERYTHING_IPC_SORT_NAME_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_NAME_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_PATH_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_PATH_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_SIZE_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_SIZE_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_EXTENSION_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_EXTENSION_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_TYPE_NAME_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_TYPE_NAME_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_CREATED_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_CREATED_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_MODIFIED_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_MODIFIED_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_ATTRIBUTES_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_ATTRIBUTES_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_RUN_COUNT_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_RUN_COUNT_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_ACCESSED_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_ACCESSED_DESCENDING;
			break;
			
		case EVERYTHING_IPC_SORT_DATE_RUN_ASCENDING:
			es_sort = EVERYTHING_IPC_SORT_DATE_RUN_DESCENDING;
			break;
	}
}

int es_find_column(int type)
{
	int i;
	
	for(i=0;i<es_numcolumns;i++)
	{
		if (es_columns[i] == type)
		{
			return i;
		}
	}

	return -1;
}
		
void es_add_column(int type)
{
	es_remove_column(type);
	
	es_columns[es_numcolumns++] = type;
}

void es_remove_column(type)
{
	int i;
	int newcount;
	
	newcount = 0;
	
	for(i=0;i<es_numcolumns;i++)
	{
		if (es_columns[i] != type)
		{
			es_columns[newcount++] = es_columns[i];
		}
	}
	
	es_numcolumns = newcount;
}
		
// TODO: FIXME: unaligned access...
void *es_get_column_data(EVERYTHING_IPC_LIST2 *list,int index,int type)
{	
	char *p;
	EVERYTHING_IPC_ITEM2 *items;
	
	items = (EVERYTHING_IPC_ITEM2 *)(list + 1);
	
	p = ((char *)list) + items[index].data_offset;

	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_NAME)
	{
		DWORD len;

		if (type == ES_COLUMN_NAME)	
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
		
		if (type == ES_COLUMN_PATH)	
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
		
		if (type == ES_COLUMN_FILENAME)	
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
		
		if (type == ES_COLUMN_EXTENSION)	
		{
			return p;
		}
		
		len = *(DWORD *)p;
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_SIZE)
	{
		if (type == ES_COLUMN_SIZE)	
		{
			return p;
		}
		
		p += sizeof(LARGE_INTEGER);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_CREATED)
	{
		if (type == ES_COLUMN_DATE_CREATED)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_MODIFIED)
	{
		if (type == ES_COLUMN_DATE_MODIFIED)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_ACCESSED)
	{
		if (type == ES_COLUMN_DATE_ACCESSED)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_ATTRIBUTES)
	{
		if (type == ES_COLUMN_ATTRIBUTES)	
		{
			return p;
		}
		
		p += sizeof(DWORD);
	}
		
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_FILE_LIST_FILE_NAME)
	{
		DWORD len;
		
		if (type == ES_COLUMN_FILE_LIST_FILENAME)	
		{
			return p;
		}
		
		len = *(DWORD *)p;
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}	
		
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_RUN_COUNT)
	{
		if (type == ES_COLUMN_RUN_COUNT)	
		{
			return p;
		}
		
		p += sizeof(DWORD);
	}	
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_RUN)
	{
		if (type == ES_COLUMN_DATE_RUN)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}		
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_RECENTLY_CHANGED)
	{
		if (type == ES_COLUMN_DATE_RECENTLY_CHANGED)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}	
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_NAME)
	{
		DWORD len;
		
		if (type == ES_COLUMN_HIGHLIGHTED_NAME)	
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
		
		if (type == ES_COLUMN_HIGHLIGHTED_PATH)	
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
		
		if (type == ES_COLUMN_HIGHLIGHTED_FILENAME)	
		{
			return p;
		}
		
		len = *(DWORD *)p;
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}			
	
	return 0;
}

void es_format_dir(wchar_t *buf)
{
	es_wstring_copy(buf,L"<DIR>");
	
//	es_space_to_width(buf,es_size_width);
}

void es_format_size(wchar_t *buf,ES_UINT64 size)
{
	if (size != 0xffffffffffffffffI64)
	{
		if (es_size_format == 0)
		{
			// auto size.
			if (size < 1000)
			{
				es_wstring_print_UINT64(buf,size);
				es_wstring_cat(buf,L"  B");
			}
			else
			{
				const wchar_t *suffix;
				
				// get suffix
				if (size / 1024I64 < 1000)
				{
					size = ((size * 100) ) / 1024;
					
					suffix = L" KB";
				}
				else
				if (size / (1024I64*1024I64) < 1000)
				{
					size = ((size * 100) ) / 1048576;
					
					suffix = L" MB";
				}
				else
				if (size / (1024I64*1024I64*1024I64) < 1000)
				{
					size = ((size * 100) ) / (1024I64*1024I64*1024I64);
					
					suffix = L" GB";
				}
				else
				if (size / (1024I64*1024I64*1024I64*1024I64) < 1000)
				{
					size = ((size * 100) ) / (1024I64*1024I64*1024I64*1024I64);
					
					suffix = L" TB";
				}
				else
				{
					size = ((size * 100) ) / (1024I64*1024I64*1024I64*1024I64*1024I64);
					
					suffix = L" PB";
				}
				
				*buf = 0;
				
				if (size == 0)
				{
					es_wstring_cat_UINT64(buf,size);
					es_wstring_cat(buf,suffix);
				}
				else
				if (size < 10)
				{
					// 0.0x
					es_wstring_cat(buf,L"0.0");
					es_wstring_cat_UINT64(buf,size);
					es_wstring_cat(buf,suffix);
				}
				else
				if (size < 100)
				{
					// 0.xx
					es_wstring_cat(buf,L"0.");
					es_wstring_cat_UINT64(buf,size);
					es_wstring_cat(buf,suffix);
				}
				else
				if (size < 1000)
				{
					// x.xx
					es_wstring_cat_UINT64(buf,size/100);
					es_wstring_cat(buf,L".");
					if (size%100 < 10)
					{
						// leading zero
						es_wstring_cat_UINT64(buf,0);
					}
					es_wstring_cat_UINT64(buf,size%100);
					es_wstring_cat(buf,suffix);
				}
				else
				if (size < 10000)
				{
					// xx.x
					es_wstring_cat_UINT64(buf,size/100);
					es_wstring_cat(buf,L".");
					es_wstring_cat_UINT64(buf,(size/10)%10);
					es_wstring_cat(buf,suffix);
				}
				else
				if (size < 100000)
				{
					// xxx
					es_wstring_cat_UINT64(buf,size/100);
					es_wstring_cat(buf,suffix);
				}
				else
				{
					// too big..
					es_format_number(buf,size/100);
					es_wstring_cat(buf,suffix);				
				}
			}
		}
		else
		if (es_size_format == 2)
		{
			es_format_number(buf,((size) + 1023) / 1024);
			es_wstring_cat(buf,L" KB");
		}
		else
		if (es_size_format == 3)
		{
			es_format_number(buf,((size) + 1048575) / 1048576);
			es_wstring_cat(buf,L" MB");
		}
		else
		if (es_size_format == 4)
		{
			es_format_number(buf,((size) + 1073741823) / 1073741824);
			es_wstring_cat(buf,L" GB");
		}
		else
		{
			es_format_number(buf,size);
		}
	}
	else
	{
		*buf = 0;
	}

//	es_format_leading_space(buf,es_size_width,es_size_leading_zero);
}

void es_format_leading_space(wchar_t *buf,int size,int ch)
{
	int len;
	
	len = es_wstring_length(buf);

	if (es_digit_grouping)
	{
		ch = ' ';
	}

	if (len < size)
	{
		int i;
		
		MoveMemory(buf+(size-len),buf,(len + 1) * sizeof(wchar_t));
		
		for(i=0;i<size-len;i++)
		{
			buf[i] = ch;
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

void es_format_attributes(wchar_t *buf,DWORD attributes)
{
	wchar_t *d;
	
	d = buf;
	
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

	*d = 0;

//	es_space_to_width(buf,es_attributes_width);
}

void es_format_run_count(wchar_t *buf,DWORD run_count)
{
	es_format_number(buf,run_count);

//	es_format_leading_space(buf,es_run_count_width,es_run_count_leading_zero);
}

void es_format_filetime(wchar_t *buf,ES_UINT64 filetime)
{
	if (filetime != 0xffffffffffffffffI64)
	{
		switch(es_date_format)
		{	
			default:
			case 0: // system format
			{
				wchar_t dmybuf[ES_WSTRING_SIZE];
				int dmyformat;
				SYSTEMTIME st;
				int val1;
				int val2;
				int val3;
								
				dmyformat = 1;

				if (GetLocaleInfoW(LOCALE_USER_DEFAULT,LOCALE_IDATE,dmybuf,ES_WSTRING_SIZE))
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

				wsprintf(buf,L"%02d/%02d/%02d %02d:%02d",val1,val2,val3,st.wHour,st.wMinute);
	//seconds		wsprintf(buf,L"%02d/%02d/%02d %02d:%02d:%02d",val1,val2,val3,st.wHour,st.wMinute,st.wSecond);
				break;
			}
				
			case 1: // ISO-8601
			{
				SYSTEMTIME st;
				es_filetime_to_localtime(&st,filetime);
				wsprintf(buf,L"%04d-%02d-%02dT%02d:%02d:%02d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
				break;
			}

			case 2: // raw filetime
				wsprintf(buf,L"%I64u",filetime);
				break;
				
			case 3: // ISO-8601 (UTC/Z)
			{
				SYSTEMTIME st;
				FileTimeToSystemTime((FILETIME *)&filetime,&st);
				wsprintf(buf,L"%04d-%02d-%02dT%02d:%02d:%02dZ",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
				break;
			}
		}
	}
	else
	{
		*buf = 0;
	}
}

void es_wstring_print_UINT64(wchar_t *buf,ES_UINT64 number)
{
	wchar_t *d;
	
	d = buf + ES_WSTRING_SIZE;
	*--d = 0;

	if (number)
	{
		ES_UINT64 i;
		
		i = number;
		
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
	
	MoveMemory(buf,d,((buf + ES_WSTRING_SIZE) - d) * sizeof(wchar_t));
}

void es_format_number(wchar_t *buf,ES_UINT64 number)
{
	wchar_t *d;
	int comma;
	
	d = buf + ES_WSTRING_SIZE;
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
		
			*--d = (wchar_t)('0' + (i % 10));
			
			i /= 10;
			
			comma++;
		}
	}
	else
	{
		*--d = '0';
	}	
	
	MoveMemory(buf,d,((buf + ES_WSTRING_SIZE) - d) * sizeof(wchar_t));
}

void es_space_to_width(wchar_t *buf,int wide)
{
	int len;
	
	len = es_wstring_length(buf);
	
	if (len < wide)
	{
		int i;
		
		for(i=0;i<wide-len;i++)
		{
			buf[i+len] = ' ';
		}
		
		buf[i+len] = 0;
	}
}

void *es_mem_alloc(uintptr_t size)
{
	void *p;

	p = HeapAlloc(GetProcessHeap(),0,size);
	if (!p)
	{
		es_fatal(ES_ERROR_OUT_OF_MEMORY);
	}
	
	return p;
}

void es_mem_free(void *ptr)
{
	HeapFree(GetProcessHeap(),0,ptr);
}

int es_is_ws(const wchar_t c)
{
	if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))
	{
		return 1;
	}
	
	return 0;
}

const wchar_t *es_skip_ws(const wchar_t *p)
{
	while(*p)
	{
		if (!es_is_ws(*p))
		{
			break;
		}
		
		p++;
	}
	
	return p;
}

const wchar_t *es_get_argv(const wchar_t *command_line)
{
	int pass;
	int inquote;
	wchar_t *d;
	
	if (es_argv)
	{
		es_mem_free(es_argv);
		
		es_argv = 0;
	}
	
	if (!*command_line)
	{
		return 0;
	}
	
	d = 0;
	
	for(pass=0;pass<2;pass++)
	{
		const wchar_t *p;

		p = es_skip_ws(command_line);
		
		inquote = 0;
		
		while(*p)
		{
			if ((!inquote) && (es_is_ws(*p)))
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
			return p;
		}
		else
		{
			d = es_mem_alloc((uintptr_t)(d+1));
			es_argv = d;
		}		
	}
	
	return 0;
}

// like es_get_argv, but we remove double quotes.
const wchar_t *es_get_command_argv(const wchar_t *command_line)
{
	int pass;
	int inquote;
	wchar_t *d;
	
	if (es_argv)
	{
		es_mem_free(es_argv);
		
		es_argv = 0;
	}
	
	if (!*command_line)
	{
		return 0;
	}
	
	d = 0;
	
	for(pass=0;pass<2;pass++)
	{
		const wchar_t *p;

		p = es_skip_ws(command_line);
		
		inquote = 0;
		
		while(*p)
		{
			if ((!inquote) && (es_is_ws(*p)))
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
			return p;
		}
		else
		{
			d = es_mem_alloc((uintptr_t)(d+1));
			es_argv = d;
		}		
	}
	
	return 0;
}

void es_set_color(int id)
{
	es_column_color[id] = es_wstring_to_int(es_argv);
	es_column_color_is_valid[id] = 1;
}

void es_set_column_wide(int id)
{
	es_column_widths[id] = es_wstring_to_int(es_argv);
	
	// sanity.
	if (es_column_widths[id] < 0) 
	{
		es_column_widths[id] = 0;
	}
	
	if (es_column_widths[id] > 200) 
	{
		es_column_widths[id] = 200;
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

int es_get_ini_filename(char *buf)
{
	char exe_filename[MAX_PATH];
	
	if (GetModuleFileNameA(0,exe_filename,MAX_PATH))
	{
		if (PathRemoveFileSpecA(exe_filename))
		{
			if (PathCombineA(buf,exe_filename,"es.ini"))
			{
				return 1;
			}
		}
	}
	
	return 0;
}

void es_ini_write_int(const char *name,int value,const char *filename)
{
	char numbuf[256];
	
	sprintf(numbuf,"%u",value);

	WritePrivateProfileStringA("ES",name,numbuf,filename);
}

void es_ini_write_string(const char *name,const wchar_t *value,const char *filename)
{
	int len;
	
	len = WideCharToMultiByte(CP_UTF8,0,value,-1,0,0,0,0);
	if (len)
	{	
		es_buf_t cbuf;
		
		es_buf_init(&cbuf,len);

		if (WideCharToMultiByte(CP_UTF8,0,value,-1,cbuf.buf,len,0,0))
		{
			WritePrivateProfileStringA("ES",name,cbuf.buf,filename);
		}

		es_buf_kill(&cbuf);
	}
}

void es_save_settings(void)
{
	char filename[ES_WSTRING_SIZE];
	
	if (es_get_ini_filename(filename))
	{
		es_ini_write_int("sort",es_sort,filename);
		es_ini_write_int("sort_ascending",es_sort_ascending,filename);
		es_ini_write_string("instance",es_instance,filename);
		es_ini_write_int("highlight_color",es_highlight_color,filename);
		es_ini_write_int("highlight",es_highlight,filename);
		es_ini_write_int("match_whole_word",es_match_whole_word,filename);
		es_ini_write_int("match_path",es_match_path,filename);
		es_ini_write_int("match_case",es_match_case,filename);
		es_ini_write_int("match_diacritics",es_match_diacritics,filename);

		{
			wchar_t columnbuf[ES_WSTRING_SIZE];
			int columni;
			
			*columnbuf = 0;
			
			for(columni=0;columni<es_numcolumns;columni++)
			{
				if (columni)
				{
					es_wstring_cat(columnbuf,L",");
				}
				
				es_wstring_cat_UINT64(columnbuf,es_columns[columni]);
			}

			es_ini_write_string("columns",columnbuf,filename);
		}

		es_ini_write_int("size_leading_zero",es_size_leading_zero,filename);
		es_ini_write_int("run_count_leading_zero",es_run_count_leading_zero,filename);
		es_ini_write_int("digit_grouping",es_digit_grouping,filename);
		es_ini_write_int("offset",es_offset,filename);
		es_ini_write_int("max_results",es_max_results,filename);
		es_ini_write_int("timeout",es_timeout,filename);

		{
			wchar_t colorbuf[ES_WSTRING_SIZE];
			int columni;
			
			*colorbuf = 0;
			
			for(columni=0;columni<ES_COLUMN_TOTAL;columni++)
			{
				if (columni)
				{
					es_wstring_cat(colorbuf,L",");
				}
				
				if (es_column_color_is_valid[columni])
				{
					es_wstring_cat_UINT64(colorbuf,es_column_color[columni]);
				}
			}

			es_ini_write_string("column_colors",colorbuf,filename);
		}

		es_ini_write_int("size_format",es_size_format,filename);
		es_ini_write_int("date_format",es_date_format,filename);
		es_ini_write_int("pause",es_pause,filename);
		es_ini_write_int("empty_search_help",es_empty_search_help,filename);
		es_ini_write_int("hide_empty_search_results",es_hide_empty_search_results,filename);
		
		{
			wchar_t widthbuf[ES_WSTRING_SIZE];
			int columni;
			
			*widthbuf = 0;
			
			for(columni=0;columni<ES_COLUMN_TOTAL;columni++)
			{
				if (columni)
				{
					es_wstring_cat(widthbuf,L",");
				}
				
				es_wstring_cat_UINT64(widthbuf,es_column_widths[columni]);
			}

			es_ini_write_string("column_widths",widthbuf,filename);
		}
	}
}

int es_ini_read_string(const char *name,wchar_t *pvalue,const char *filename)
{
	char buf[ES_WSTRING_SIZE];
	char default_string[ES_WSTRING_SIZE];
	
	default_string[0] = 0;
	
	if (GetPrivateProfileStringA("ES",name,default_string,buf,ES_WSTRING_SIZE,filename))
	{
		if (*buf)
		{
			if (MultiByteToWideChar(CP_UTF8,0,buf,-1,pvalue,ES_WSTRING_SIZE))
			{
				return 1;
			}
		}
	}
	
	return 0;
}

int es_ini_read_int(const char *name,int *pvalue,const char *filename)
{
	wchar_t wbuf[ES_WSTRING_SIZE];
	
	if (es_ini_read_string(name,wbuf,filename))
	{	
		*pvalue = es_wstring_to_int(wbuf);
		
		return 1;
	}
	
	return 0;
}


void es_load_settings(void)
{
	char filename[ES_WSTRING_SIZE];
	
	if (es_get_ini_filename(filename))
	{
		es_ini_read_int("sort",&es_sort,filename);
		es_ini_read_int("sort_ascending",&es_sort_ascending,filename);
		es_ini_read_string("instance",es_instance,filename);
		es_ini_read_int("highlight_color",&es_highlight_color,filename);
		es_ini_read_int("highlight",&es_highlight,filename);
		es_ini_read_int("match_whole_word",&es_match_whole_word,filename);
		es_ini_read_int("match_path",&es_match_path,filename);
		es_ini_read_int("match_case",&es_match_case,filename);
		es_ini_read_int("match_diacritics",&es_match_diacritics,filename);

		{
			wchar_t columnbuf[ES_WSTRING_SIZE];
			wchar_t *p;
			
			*columnbuf = 0;
			
			es_ini_read_string("columns",columnbuf,filename);
			
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
					columntype = es_wstring_to_int(start);
					
					if ((columntype >= 0) && (columntype < ES_COLUMN_TOTAL))
					{
						es_add_column(columntype);
					}
				}
			}
		}

		es_ini_read_int("size_leading_zero",&es_size_leading_zero,filename);
		es_ini_read_int("run_count_leading_zero",&es_run_count_leading_zero,filename);
		es_ini_read_int("digit_grouping",&es_digit_grouping,filename);
		es_ini_read_int("offset",&es_offset,filename);
		es_ini_read_int("max_results",&es_max_results,filename);
		es_ini_read_int("timeout",&es_timeout,filename);

		{
			wchar_t colorbuf[ES_WSTRING_SIZE];
			wchar_t *p;
			int column_index;
			
			*colorbuf = 0;
			
			es_ini_read_string("column_colors",colorbuf,filename);
			
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
					color = es_wstring_to_int(start);
					
					es_column_color[column_index] = color;
					es_column_color_is_valid[column_index] = 1;
				}
				
				column_index++;
			}
		}

		es_ini_read_int("size_format",&es_size_format,filename);
		es_ini_read_int("date_format",&es_date_format,filename);
		es_ini_read_int("pause",&es_pause,filename);
		es_ini_read_int("empty_search_help",&es_empty_search_help,filename);
		es_ini_read_int("hide_empty_search_results",&es_hide_empty_search_results,filename);
		
		{
			wchar_t widthbuf[ES_WSTRING_SIZE];
			wchar_t *p;
			int column_index;
			
			*widthbuf = 0;
			
			es_ini_read_string("column_widths",widthbuf,filename);
			
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
					width = es_wstring_to_int(start);
					
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
	}
}

void es_append_filter(const wchar_t *filter)
{
	if (*es_filter)
	{
		es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,L" ");
	}

	es_wbuf_cat(es_filter,ES_FILTER_BUF_SIZE,filter);
}

// max MUST be > 0
void es_wbuf_cat(wchar_t *buf,int max,const wchar_t *s)
{
	const wchar_t *p;
	wchar_t *d;
	
	max--;
	d = buf;
	
	while(max)
	{
		if (!*d) break;
		
		d++;
		max--;
	}
	
	p = s;
	while(max)
	{
		if (!*p) break;
		
		*d++ = *p;
		p++;
		max--;
	}
	
	*d = 0;
}

wchar_t *es_wstring_alloc(const wchar_t *s)
{
	int size;
	wchar_t *p;
	
	size = (es_wstring_length(s) + 1) * sizeof(wchar_t);
	
	p = es_mem_alloc(size);
	
	es_copy_memory(p,s,size);
	
	return p;
}

void es_do_run_history_command(void)
{	
	es_everything_hwnd = es_find_ipc_window();
	if (es_everything_hwnd)
	{
		COPYDATASTRUCT cds;
		DWORD run_count;

		cds.cbData = es_run_history_size;
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
		// the everything window was not found.
		// we can optionally RegisterWindowMessage("EVERYTHING_IPC_CREATED") and 
		// wait for Everything to post this message to all top level windows when its up and running.
		es_fatal(ES_ERROR_IPC);
	}
}

// checks for -sort-size, -sort-size-ascending and -sort-size-descending
int es_check_sorts(void)
{
	wchar_t sortnamebuf[ES_WSTRING_SIZE];
	int sortnamei;
	
	for(sortnamei=0;sortnamei<ES_SORT_NAME_COUNT;sortnamei++)
	{
		es_wstring_copy(sortnamebuf,L"sort-");
		es_wstring_cat(sortnamebuf,es_sort_names[sortnamei]);
		es_wstring_cat(sortnamebuf,L"-ascending");
		
		if (es_check_param(es_argv,sortnamebuf))
		{
			es_sort = es_sort_names_to_ids[sortnamei];
			es_sort_ascending = 1;

			return 1;
		}
		
		es_wstring_copy(sortnamebuf,L"sort-");
		es_wstring_cat(sortnamebuf,es_sort_names[sortnamei]);
		es_wstring_cat(sortnamebuf,L"-descending");
		
		if (es_check_param(es_argv,sortnamebuf))
		{
			es_sort = es_sort_names_to_ids[sortnamei];
			es_sort_ascending = -1;

			return 1;
		}
		
		es_wstring_copy(sortnamebuf,L"sort-");
		es_wstring_cat(sortnamebuf,es_sort_names[sortnamei]);
		
		if (es_check_param(es_argv,sortnamebuf))
		{
			es_sort = es_sort_names_to_ids[sortnamei];

			return 1;
		}
	}
	
	return 0;
}

static void *es_buf_init(es_buf_t *cbuf,uintptr_t size)
{
	if (size <= ES_BUF_SIZE)
	{
		cbuf->buf = cbuf->stackbuf;
	}
	else
	{
		cbuf->buf = es_mem_alloc(size);
	}
	
	return cbuf->buf;
}

static void es_buf_kill(es_buf_t *cbuf)
{
	if (cbuf->buf != cbuf->stackbuf)
	{
		es_mem_free(cbuf->buf);
	}
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

// initialize a wchar buffer.
// the default value is an empty string.
static void es_wchar_buf_init(es_wchar_buf_t *wcbuf)
{
	wcbuf->buf = wcbuf->stack_buf;
	wcbuf->length_in_wchars = 0;
	wcbuf->size_in_wchars = ES_WCHAR_BUF_STACK_SIZE;
	wcbuf->buf[0] = 0;
}

// kill a wchar buffer.
// returns any allocated memory back to the system.
static void es_wchar_buf_kill(es_wchar_buf_t *wcbuf)
{
	if (wcbuf->buf != wcbuf->stack_buf)
	{
		es_mem_free(wcbuf->buf);
	}
}

// empty a wchar buffer.
// the wchar buffer is set to an empty string.
static void es_wchar_buf_empty(es_wchar_buf_t *wcbuf)
{
	es_wchar_buf_kill(wcbuf);
	es_wchar_buf_init(wcbuf);
}

// doesn't keep the existing text.
// doesn't set the text.
// doesn't set length.
// caller should set the text.
// returns FALSE on error. Call GetLastError() for more information.
// returns TRUE if successful.
static void es_wchar_buf_grow_size(es_wchar_buf_t *wcbuf,SIZE_T size_in_wchars)
{
	if (size_in_wchars <= wcbuf->size_in_wchars)
	{
		// already enough room.
	}
	else
	{
		es_wchar_buf_empty(wcbuf);

		wcbuf->buf = es_mem_alloc(es_safe_size_add(size_in_wchars,size_in_wchars));
		wcbuf->size_in_wchars = size_in_wchars;
	}
}

// doesn't keep the existing text.
// doesn't set the text, only sets the length.
// DOES set the length.
// caller should set the text.
// returns FALSE on error. Call GetLastError() for more information.
// returns TRUE if successful.
static void es_wchar_buf_grow_length(es_wchar_buf_t *wcbuf,SIZE_T length_in_wchars)
{
	es_wchar_buf_grow_size(wcbuf,es_safe_size_add(length_in_wchars,1));

	wcbuf->length_in_wchars = length_in_wchars;
}

// cat a wide string to another wide string.
// call with a NULL buf to calculate the size.
// returns required length in wide chars. (not bytes)
static wchar_t *es_wchar_string_cat_wchar_string_no_null_terminate(wchar_t *buf,wchar_t *current_d,const wchar_t *s)
{
	const wchar_t *p;
	wchar_t *d;

	p = s;
	d = current_d;
	
	while(*p)
	{
		if (buf)
		{
			*d++ = *p;
		}
		else
		{
			d = (void *)es_safe_size_add((SIZE_T)d,1);
		}
	
		p++;
	}
	
	return d;
}

// get pipe name.
// instance_name can be NULL.
static wchar_t *es_get_pipe_name(wchar_t *buf,const wchar_t *instance_name)
{
	wchar_t *d;
	
	d = buf;
	
	d = es_wchar_string_cat_wchar_string_no_null_terminate(buf,d,L"\\\\.\\PIPE\\Everything IPC");

	if ((instance_name) && (*instance_name))
	{
		d = es_wchar_string_cat_wchar_string_no_null_terminate(buf,d,L" (");
		d = es_wchar_string_cat_wchar_string_no_null_terminate(buf,d,instance_name);
		d = es_wchar_string_cat_wchar_string_no_null_terminate(buf,d,L")");
	}

	if (buf)	
	{
		*d = 0;
	}
	
	return d;
}

// instance_name must be non-NULL.
static void es_wchar_buf_get_pipe_name(es_wchar_buf_t *wcbuf,const wchar_t *instance_name)
{
	es_wchar_buf_grow_length(wcbuf,(SIZE_T)es_get_pipe_name(NULL,instance_name));

	es_get_pipe_name(wcbuf->buf,instance_name);
}

// safely add some values.
// SIZE_MAX is used as an invalid value.
// you will be unable to allocate SIZE_MAX bytes.
// always use es_safe_size when allocating memory.
SIZE_T es_safe_size_add(SIZE_T a,SIZE_T b)
{
	SIZE_T c;
	
	c = a + b;
	
	if (c < a) 
	{
		return SIZE_MAX;
	}
	
	return c;
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
	return es_copy_memory(buf,&value,sizeof(DWORD));
}

static BYTE *es_copy_uint64(BYTE *buf,ES_UINT64 value)
{
	return es_copy_memory(buf,&value,sizeof(ES_UINT64));
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

// src or dst can be unaligned.
// returns the dst + size.
static void *es_copy_memory(void *dst,const void *src,SIZE_T size)
{
	BYTE *d;
	
	d = dst;
	
	CopyMemory(d,src,size);
	
	return d + size;
}

// src or dst can be unaligned.
// returns the dst + size.
static void *es_zero_memory(void *dst,SIZE_T size)
{
	BYTE *d;
	
	d = dst;
	
	ZeroMemory(d,size);
	
	return d + size;
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
				es_zero_memory(d,run);
				
				stream->is_error = 1;
			
				return;
			}
			
			if (!es_read_pipe(stream->pipe_handle,&recv_header,sizeof(_everything3_message_t)))
			{
				// read header failed.
				es_zero_memory(d,run);
				
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
				es_zero_memory(d,run);
				
				stream->is_error = 1;
				
				return;
			}
			
			if (recv_header.size)			
			{
//TODO: dont reallocate.
				if (stream->buf)
				{
					es_mem_free(stream->buf);
				}
			
				stream->buf = es_mem_alloc(recv_header.size);
				if (!stream->buf)
				{
					es_zero_memory(d,run);
					
					stream->is_error = 1;
					
					break;
				}
						
				if (!es_read_pipe(stream->pipe_handle,stream->buf,recv_header.size))
				{
					es_zero_memory(d,run);
					
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
	start = es_safe_size_add(start,0xff);
	
	word_value = _everything3_stream_read_word(stream);
	
	if (word_value < 0xffff)
	{
		return es_safe_size_add(start,word_value);
	}	
	
	// DWORD
	start = es_safe_size_add(start,0xffff);

	dword_value = _everything3_stream_read_dword(stream);
	
	if (dword_value < 0xffffffff)
	{
		return es_safe_size_add(start,dword_value);
	}

#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64

	{
		ES_UINT64 uint64_value;
		
		// UINT64
		start = es_safe_size_add(start,0xffffffff);

		uint64_value = _everything3_stream_read_uint64(stream);

		if (uint64_value < 0xFFFFFFFFFFFFFFFFUI64)
		{
			return es_safe_size_add(start,uint64_value);
		}

		stream->is_error = 1;
		return ES_UINT64_MAX;
	}
	
#elif SIZE_MAX == 0xffffffffui32
		
	stream->error_code = EVERYTHING3_ERROR_OUT_OF_MEMORY;
	return EVERYTHING3_DWORD_MAX;

#else

	#error unknown UINTPTR_MAX

#endif
}

// Init a utf8 buffer to an empty string.
static void es_utf8_buf_init(es_utf8_buf_t *cbuf)
{
	cbuf->buf = cbuf->stack_buf;
	cbuf->length_in_bytes = 0;
	cbuf->size_in_bytes = ES_UTF8_BUF_STACK_SIZE;
	cbuf->buf[0] = 0;
}

// Kill the UTF-8 buffer, releasing any allocated memory back to the system.
static void es_utf8_buf_kill(es_utf8_buf_t *cbuf)
{
	if (cbuf->buf != cbuf->stack_buf)
	{
		es_mem_free(cbuf->buf);
	}
}

// Empty the UTF-8 buffer, the buffer will be set to an empty string.
static void es_utf8_buf_empty(es_utf8_buf_t *cbuf)
{
	es_utf8_buf_kill(cbuf);
	es_utf8_buf_init(cbuf);
}

// doesn't keep the existing text.
// doesn't set the text, only sets the length.
static void es_utf8_buf_grow_size(es_utf8_buf_t *cbuf,SIZE_T size_in_bytes)
{
	if (size_in_bytes <= cbuf->size_in_bytes)
	{
		// already enough room
	}
	else
	{
		es_utf8_buf_empty(cbuf);
		
		cbuf->buf = es_mem_alloc(size_in_bytes);
		cbuf->size_in_bytes = size_in_bytes;
	}
}

// doesn't keep the existing text.
// doesn't set the text, only sets the length.
static void es_utf8_buf_grow_length(es_utf8_buf_t *cbuf,SIZE_T length_in_bytes)
{
	es_utf8_buf_grow_size(cbuf,es_safe_size_add(length_in_bytes,1));

	cbuf->length_in_bytes = length_in_bytes;
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


// Copy a wide char string into a UTF-8 buffer.
// Use a NULL buffer to calculate the size.
// Handles surrogates correctly.
static ES_UTF8 *es_utf8_string_copy_wchar_string(ES_UTF8 *buf,const wchar_t *ws)
{
	const wchar_t *p;
	ES_UTF8 *d;
	int c;
	
	p = ws;
	d = buf;
	
	while(*p)
	{
		c = *p++;
		
		// surrogates
		if ((c >= 0xD800) && (c < 0xDC00))
		{
			if ((*p >= 0xDC00) && (*p < 0xE000))
			{
				c = 0x10000 + ((c - 0xD800) << 10) + (*p - 0xDC00);
				
				p++;
			}
		}
		
		if (c > 0xffff)
		{
			// 4 bytes
			if (buf)
			{
				*d++ = ((c >> 18) & 0x07) | 0xF0; // 11110xxx
				*d++ = ((c >> 12) & 0x3f) | 0x80; // 10xxxxxx
				*d++ = ((c >> 6) & 0x3f) | 0x80; // 10xxxxxx
				*d++ = (c & 0x3f) | 0x80; // 10xxxxxx
			}
			else
			{
				d = (void *)es_safe_size_add((SIZE_T)d,4);
			}
		}
		else
		if (c > 0x7ff)
		{
			// 3 bytes
			if (buf)
			{
				*d++ = ((c >> 12) & 0x0f) | 0xE0; // 1110xxxx
				*d++ = ((c >> 6) & 0x3f) | 0x80; // 10xxxxxx
				*d++ = (c & 0x3f) | 0x80; // 10xxxxxx
			}
			else
			{
				d = (void *)es_safe_size_add((SIZE_T)d,3);
			}
		}
		else
		if (c > 0x7f)
		{
			// 2 bytes
			if (buf)
			{
				*d++ = ((c >> 6) & 0x1f) | 0xC0; // 110xxxxx
				*d++ = (c & 0x3f) | 0x80; // 10xxxxxx
			}
			else
			{
				d = (void *)es_safe_size_add((SIZE_T)d,2);
			}
		}
		else
		{	
			// ascii
			if (buf)
			{
				*d++ = c;
			}
			else
			{
				d = (void *)es_safe_size_add((SIZE_T)d,1);
			}
		}
	}
	
	if (buf)
	{
		*d = 0;
	}
	
	return d;
}

// copy a wide string into a UTF-8 buffer.
// returns TRUE on success. Otherwise FALSE if there's not enough memory.
static void es_utf8_buf_copy_wchar_string(es_utf8_buf_t *cbuf,const wchar_t *ws)
{
	es_utf8_buf_grow_length(cbuf,(SIZE_T)es_utf8_string_copy_wchar_string(NULL,ws));

	es_utf8_string_copy_wchar_string(cbuf->buf,ws);
}
