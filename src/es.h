
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

// errorlevel returned from ES.
#define ES_ERROR_SUCCESS					0 // no known error, search successful.
#define ES_ERROR_REGISTER_WINDOW_CLASS		1 // failed to register window class
#define ES_ERROR_CREATE_WINDOW				2 // failed to create listening window.
#define ES_ERROR_OUT_OF_MEMORY				3 // out of memory
#define ES_ERROR_EXPECTED_SWITCH_PARAMETER	4 // expected an additional command line option with the specified switch or bad switch param
#define ES_ERROR_CREATE_FILE				5 // failed to create export output file
#define ES_ERROR_UNKNOWN_SWITCH				6 // unknown switch.
#define ES_ERROR_IPC_ERROR					7 // failed to send Everything IPC a query or bad IPC reply.
#define ES_ERROR_NO_IPC						8 // NO Everything IPC window or pipe.
#define ES_ERROR_NO_RESULTS					9 // No results found. Only set if -no-result-error is used

#define ES_UINT64_MAX		0xffffffffffffffffUI64
#define ES_DWORD_MAX		0xffffffff
#define ES_WORD_MAX			0xffff
#define ES_BYTE_MAX			0xff
//#define INT64_MIN	(-9223372036854775807i64 - 1)
//#define INT64_MAX	0x7fffffffffffffffi64
//#define INT32_MIN	(-2147483647 - 1)
//#define INT32_MAX	2147483647

#define ES_IPC_VERSION_FLAG_IPC1	0x00000001 // Everything 1.3
#define ES_IPC_VERSION_FLAG_IPC2	0x00000002 // Everything 1.4
#define ES_IPC_VERSION_FLAG_IPC3	0x00000004 // Everything 1.5

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <limits.h> // SIZE_MAX

typedef unsigned __int64 ES_UINT64;
typedef BYTE ES_UTF8;

#include "everything_ipc.h"
#include "everything3.h"
#include "version.h"
#include "safe_size.h"
#include "safe_int.h"
#include "mem.h"
#include "pool.h"
#include "array.h"
#include "unicode.h"
#include "wchar_string.h"
#include "wchar_buf.h"
#include "utf8_string.h"
#include "utf8_buf.h"
#include "config.h"
#include "property.h"
#include "property_unknown.h"
#include "column.h"
#include "column_color.h"
#include "column_width.h"
#include "secondary_sort.h"
#include "os.h"
#include "debug.h"
#include "ipc3.h"

void DECLSPEC_NORETURN es_fatal(int error_code);

extern wchar_buf_t *es_instance_name_wcbuf;
extern DWORD es_timeout;
extern DWORD es_ipc_version; // allow all ipc versions
extern int es_pixels_to_characters_mul;
extern int es_pixels_to_characters_div;
