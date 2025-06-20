
// compiler options
//TODO: remove.
#pragma warning(disable : 4996) // deprecation
#pragma warning(disable : 4311) // warning C4311: 'type cast' : pointer truncation from 'const void *' to 'DWORD'
#pragma warning(disable : 4312) // warning C4312: 'type cast' : conversion from 'DWORD' to 'const void *' of greater size

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

//TODO: remove.
//#define ES_WSTRING_SIZE				MAX_PATH

#define ES_DWORD_MAX		0xffffffff
#define ES_UINT64_MAX		0xffffffffffffffffUI64

#include <windows.h>
//#include <stdio.h>

typedef unsigned __int64 ES_UINT64;
typedef BYTE ES_UTF8;

// IPC Header from the Everything SDK:
#include "everything_ipc.h"
#include "everything3.h"
//#include "shlwapi.h" // Path functions
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
#include "os.h"

void DECLSPEC_NORETURN es_fatal(int error_code);
