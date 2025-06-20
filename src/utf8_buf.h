
#define UTF8_BUF_STACK_SIZE		MAX_PATH

// a dyanimcally sized UTF-8 string
// has some stack space to avoid memory allocations.
typedef struct utf8_buf_s
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
	ES_UTF8 stack_buf[UTF8_BUF_STACK_SIZE];
	
}utf8_buf_t;

void utf8_buf_init(utf8_buf_t *cbuf);
void utf8_buf_kill(utf8_buf_t *cbuf);
void utf8_buf_empty(utf8_buf_t *cbuf);
void utf8_buf_grow_size(utf8_buf_t *cbuf,SIZE_T size_in_bytes);
void utf8_buf_grow_length(utf8_buf_t *cbuf,SIZE_T length_in_bytes);
void utf8_buf_copy_wchar_string(utf8_buf_t *cbuf,const wchar_t *ws);
void utf8_buf_vprintf(utf8_buf_t *cbuf,const ES_UTF8 *format,va_list argptr);
void utf8_buf_printf(utf8_buf_t *cbuf,const ES_UTF8 *format,...);
