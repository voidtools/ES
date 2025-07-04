
// IPC pipe commands.
#define IPC3_COMMAND_GET_IPC_PIPE_VERSION			0
#define IPC3_COMMAND_GET_MAJOR_VERSION				1
#define IPC3_COMMAND_GET_MINOR_VERSION				2
#define IPC3_COMMAND_GET_REVISION					3
#define IPC3_COMMAND_GET_BUILD_NUMBER				4
#define IPC3_COMMAND_GET_TARGET_MACHINE				5
#define IPC3_COMMAND_FIND_PROPERTY_FROM_NAME		6
#define IPC3_COMMAND_SEARCH							7
#define IPC3_COMMAND_IS_DB_LOADED					8
#define IPC3_COMMAND_IS_PROPERTY_INDEXED			9
#define IPC3_COMMAND_IS_PROPERTY_FAST_SORT			10
#define IPC3_COMMAND_GET_PROPERTY_NAME				11
#define IPC3_COMMAND_GET_PROPERTY_CANONICAL_NAME	12
#define IPC3_COMMAND_GET_PROPERTY_TYPE				13
#define IPC3_COMMAND_IS_RESULT_CHANGE				14
#define IPC3_COMMAND_GET_RUN_COUNT					15
#define IPC3_COMMAND_SET_RUN_COUNT					16
#define IPC3_COMMAND_INC_RUN_COUNT					17
#define IPC3_COMMAND_GET_FOLDER_SIZE				18
#define IPC3_COMMAND_GET_FILE_ATTRIBUTES			19
#define IPC3_COMMAND_GET_FILE_ATTRIBUTES_EX			20
#define IPC3_COMMAND_GET_FIND_FIRST_FILE			21
#define IPC3_COMMAND_GET_RESULTS					22
#define IPC3_COMMAND_SORT							23
#define IPC3_COMMAND_WAIT_FOR_RESULT_CHANGE			24
#define IPC3_COMMAND_CANCEL							25

// IPC pipe responses
#define IPC3_RESPONSE_OK_MORE_DATA					100 // expect another repsonse.
#define IPC3_RESPONSE_OK							200 // reply data depending on request.
#define IPC3_RESPONSE_ERROR_BAD_REQUEST				400
#define IPC3_RESPONSE_ERROR_CANCELLED				401 // another requested was made while processing...
#define IPC3_RESPONSE_ERROR_NOT_FOUND				404
#define IPC3_RESPONSE_ERROR_OUT_OF_MEMORY			500
#define IPC3_RESPONSE_ERROR_INVALID_COMMAND			501

#define IPC3_SEARCH_FLAG_MATCH_CASE					0x00000001	// match case
#define IPC3_SEARCH_FLAG_MATCH_WHOLEWORD			0x00000002	// match whole word
#define IPC3_SEARCH_FLAG_MATCH_PATH					0x00000004	// include paths in search
#define IPC3_SEARCH_FLAG_REGEX						0x00000008	// enable regex
#define IPC3_SEARCH_FLAG_MATCH_DIACRITICS			0x00000010	// match diacritic marks
#define IPC3_SEARCH_FLAG_MATCH_PREFIX				0x00000020	// match prefix (Everything 1.5)
#define IPC3_SEARCH_FLAG_MATCH_SUFFIX				0x00000040	// match suffix (Everything 1.5)
#define IPC3_SEARCH_FLAG_IGNORE_PUNCTUATION			0x00000080	// ignore punctuation (Everything 1.5)
#define IPC3_SEARCH_FLAG_IGNORE_WHITESPACE			0x00000100	// ignore white-space (Everything 1.5)
#define IPC3_SEARCH_FLAG_FOLDERS_FIRST_ASCENDING	0x00000000	// folders first when sort ascending
#define IPC3_SEARCH_FLAG_FOLDERS_FIRST_ALWAYS		0x00000200	// folders first
#define IPC3_SEARCH_FLAG_FOLDERS_FIRST_NEVER		0x00000400	// folders last 
#define IPC3_SEARCH_FLAG_FOLDERS_FIRST_DESCENDING	0x00000600	// folders first when sort descending
#define IPC3_SEARCH_FLAG_TOTAL_SIZE					0x00000800	// calculate total size
#define IPC3_SEARCH_FLAG_HIDE_RESULT_OMISSIONS		0x00001000	// hide omitted results
#define IPC3_SEARCH_FLAG_SORT_MIX					0x00002000	// mix file and folder results
#define IPC3_SEARCH_FLAG_64BIT						0x00004000	// SIZE_T is 64bits. Otherwise, 32bits.
#define IPC3_SEARCH_FLAG_FORCE						0x00008000	// Force a research, even when the search state doesn't change.

#define IPC3_MESSAGE_DATA(msg)						((void *)(((ipc3_message_t *)(msg)) + 1))

#define IPC3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT	0x00000001
#define IPC3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT	0x00000002

#define IPC3_RESULT_LIST_ITEM_FLAG_FOLDER			0x01
#define IPC3_RESULT_LIST_ITEM_FLAG_ROOT				0x02

// Everything3_GetResultListPropertyRequestValueType()
#define	IPC3_PROPERTY_VALUE_TYPE_NULL								0
#define	IPC3_PROPERTY_VALUE_TYPE_BYTE								1 // Everything3_GetResultPropertyBYTE
#define	IPC3_PROPERTY_VALUE_TYPE_WORD								2 // Everything3_GetResultPropertyWORD
#define	IPC3_PROPERTY_VALUE_TYPE_DWORD								3 // Everything3_GetResultPropertyDWORD
#define	IPC3_PROPERTY_VALUE_TYPE_DWORD_FIXED_Q1K					4 // Everything3_GetResultPropertyDWORD / 1000
#define	IPC3_PROPERTY_VALUE_TYPE_UINT64								5 // Everything3_GetResultPropertyUINT64
#define	IPC3_PROPERTY_VALUE_TYPE_UINT128							6 // Everything3_GetResultPropertyUINT128
#define	IPC3_PROPERTY_VALUE_TYPE_DIMENSIONS							7 // Everything3_GetResultPropertyDIMENSIONS
#define	IPC3_PROPERTY_VALUE_TYPE_PSTRING							8 // Everything3_GetResultPropertyText
#define	IPC3_PROPERTY_VALUE_TYPE_PSTRING_MULTISTRING				9 // Everything3_GetResultPropertyText
#define	IPC3_PROPERTY_VALUE_TYPE_PSTRING_STRING_REFERENCE			10 // Everything3_GetResultPropertyText
#define	IPC3_PROPERTY_VALUE_TYPE_SIZE_T								11 // Everything3_GetResultPropertySIZE_T
#define	IPC3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1K					12 // Everything3_GetResultPropertyINT32 / 1000
#define	IPC3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1M					13 // Everything3_GetResultPropertyINT32 / 1000000
#define	IPC3_PROPERTY_VALUE_TYPE_PSTRING_FOLDER_REFERENCE			14 // Everything3_GetResultPropertyText
#define	IPC3_PROPERTY_VALUE_TYPE_PSTRING_FILE_OR_FOLDER_REFERENCE	15 // Everything3_GetResultPropertyText
#define	IPC3_PROPERTY_VALUE_TYPE_BLOB8								16 // Everything3_GetResultPropertyBlob
#define	IPC3_PROPERTY_VALUE_TYPE_DWORD_GET_TEXT						17 // Everything3_GetResultPropertyDWORD
#define	IPC3_PROPERTY_VALUE_TYPE_WORD_GET_TEXT						18 // Everything3_GetResultPropertyWORD
#define	IPC3_PROPERTY_VALUE_TYPE_BLOB16								19 // Everything3_GetResultPropertyBlob
#define	IPC3_PROPERTY_VALUE_TYPE_BYTE_GET_TEXT						20 // Everything3_GetResultPropertyBYTE
#define	IPC3_PROPERTY_VALUE_TYPE_PROPVARIANT						21 // Everything3_GetResultPropertyPropVariant

#define IPC3_SEARCH_SORT_FLAG_DESCENDING				0x00000001

#define IPC3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT		0x00000001
#define IPC3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT		0x00000002

// a sort item
typedef struct ipc3_search_sort_s
{
	DWORD property_id;
	DWORD flags;
	
}ipc3_search_sort_t;

// property request item
typedef struct ipc3_search_property_request_s
{
	DWORD property_id;
	DWORD flags;
	
}ipc3_search_property_request_t;

// IPC pipe message
typedef struct ipc3_message_s
{
	DWORD code; // IPC3_COMMAND_* or IPC3_RESPONSE_*
	DWORD size; // excludes header size.
	
	// data follows
	// BYTE data[size];
	
}ipc3_message_t;

// stream virtual table.
typedef struct ipc3_stream_vtbl_s
{
	// jump to a position in the stream.
	// is_error MUST be set on seek error
	void (*seek_proc)(struct ipc3_stream_s *stream,ES_UINT64 position_from_start);
	
	// get the current position in the stream.
	ES_UINT64 (*tell_proc)(struct ipc3_stream_s *stream);
	
	// read from the stream.
	// is_error MUST be set on read error
	// buf MUST be set to zeroes on read error.
	// returns the amount of data read.
	// can return less than size.
	// set stream->is_error on any errors.
	SIZE_T (*read_proc)(struct ipc3_stream_s *stream,void *buf,SIZE_T size);

	// close the stream.
	void (*close_proc)(struct ipc3_stream_s *stream);
	
}ipc3_stream_vtbl_t;

// input stream.
typedef struct ipc3_stream_s
{
	const ipc3_stream_vtbl_t *vtbl;
	int is_error;
	int is_64bit;
	
}ipc3_stream_t;

// pipe stream.
typedef struct ipc3_stream_pipe_s
{
	ipc3_stream_t base;
	
	HANDLE pipe_handle;

	// NULL if not yet allocated
	BYTE *buf;
	BYTE *p;
	SIZE_T avail;
	int is_last;
	int is_eof;
	DWORD pipe_avail;
	DWORD buf_size;
	ES_UINT64 pipe_totread;
		
}ipc3_stream_pipe_t;

// memory stream.
typedef struct ipc3_stream_pool_s
{
	ipc3_stream_t base;
	
	// an array of ipc3_stream_pool_chunk_t *
	array_t chunk_array;
	
	// current position.
	SIZE_T chunk_cur;
	BYTE *p;
	SIZE_T avail;
	
	int is_last;
	SIZE_T last_chunk_numread;
	
	// the original source input stream.
	struct ipc3_stream_s *source_stream;
		
}ipc3_stream_pool_t;

typedef struct ipc3_result_list_property_request_s
{
	DWORD property_id;
	// one or more of IPC3_SEARCH_PROPERTY_REQUEST_FLAG_*
	DWORD flags;
	DWORD value_type;
	
}ipc3_result_list_property_request_t;

// an ipc3 result list.
typedef struct ipc3_result_list_s
{
	ES_UINT64 total_result_size;
	SIZE_T folder_result_count;
	SIZE_T file_result_count;
	SIZE_T viewport_offset;
	SIZE_T viewport_count;
	SIZE_T sort_count;

	SIZE_T property_request_count;

	DWORD valid_flags;
	
	// the pipe stream
	// -or-
	// the memory stream if we are in es_pause mode.
	ipc3_stream_t *stream;

	// ipc3_result_list_property_request_t *property_request_array;
	utf8_buf_t property_request_cbuf;
	
	// index to stream offset in bytes.
	// this array has a count of viewport_count items.
	// the first index will always be 0.
	// only used by es_pause.
	SIZE_T *index_to_stream_offset_array;
	
	// the number of valid index to stream offset items in.
	// index_to_stream_offset_array.
	// only used by es_pause.
	SIZE_T index_to_stream_offset_valid_count;
	
}ipc3_result_list_t;

BOOL ipc3_write_pipe_data(HANDLE pipe_handle,const void *in_data,SIZE_T in_size);
BOOL ipc3_write_pipe_message(HANDLE pipe_handle,DWORD code,const void *in_data,SIZE_T in_size);
void ipc3_stream_read_data(ipc3_stream_t *stream,void *data,SIZE_T size);
SIZE_T ipc3_stream_try_read_data(ipc3_stream_t *stream,void *data,SIZE_T size);
void ipc3_stream_skip(ipc3_stream_t *stream,SIZE_T size);
BYTE ipc3_stream_read_byte(ipc3_stream_t *stream);
WORD ipc3_stream_read_word(ipc3_stream_t *stream);
DWORD ipc3_stream_read_dword(ipc3_stream_t *stream);
ES_UINT64 ipc3_stream_read_uint64(ipc3_stream_t *stream);
SIZE_T ipc3_stream_read_size_t(ipc3_stream_t *stream);
SIZE_T ipc3_stream_read_len_vlq(ipc3_stream_t *stream);
BOOL ipc3_read_pipe(HANDLE pipe_handle,void *buf,SIZE_T buf_size);
BOOL ipc3_skip_pipe(HANDLE pipe_handle,SIZE_T buf_size);
HANDLE ipc3_connect_pipe(void);
BOOL ipc3_ioctl(HANDLE pipe_handle,int command,const void *in_buf,SIZE_T in_size,void *out_buf,SIZE_T out_size,SIZE_T *out_numread);
BOOL ipc3_ioctl_expect_output_size(HANDLE pipe_handle,int command,const void *in_buf,SIZE_T in_size,void *out_buf,SIZE_T out_size);
void ipc3_get_pipe_name(wchar_buf_t *out_wcbuf);
void ipc3_stream_pipe_init(ipc3_stream_pipe_t *stream,HANDLE pipe_handle);
void ipc3_stream_close(ipc3_stream_t *stream);				
BOOL ipc3_is_property_indexed(HANDLE pipe_handle,DWORD property_id);
void ipc3_result_list_init(ipc3_result_list_t *result_list,ipc3_stream_t *stream);
void ipc3_result_list_kill(ipc3_result_list_t *result_list);
void ipc3_stream_pool_init(ipc3_stream_pool_t *stream,ipc3_stream_t *source_stream);
void ipc3_stream_seek(ipc3_stream_t *stream,ES_UINT64 position_from_start);
ES_UINT64 ipc3_stream_tell(ipc3_stream_t *stream);
void ipc3_result_list_seek_to_offset_from_index(ipc3_result_list_t *result_list,SIZE_T start_index);
ES_UINT64 ipc3_stream_tell(ipc3_stream_t *stream);
BYTE *ipc3_copy_len_vlq(BYTE *buf,SIZE_T value);
DWORD ipc3_find_property(const wchar_t *search);
