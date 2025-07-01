
typedef struct config_ini_s
{
	utf8_buf_t file_cbuf;
	ES_UTF8 *p;
	const ES_UTF8 *key;
	const ES_UTF8 *value;
	pool_t pool;
	struct _config_keyvalue_s *keyvalue_start;
	struct _config_keyvalue_s *keyvalue_last;
	
}config_ini_t;

BOOL config_get_filename(int is_appdata,int is_temp,wchar_buf_t *wcbuf);
void config_write_int(HANDLE file_handle,const ES_UTF8 *name,int value);
void config_write_dword(HANDLE file_handle,const ES_UTF8 *name,DWORD value);
void config_write_uint64(HANDLE file_handle,const ES_UTF8 *name,ES_UINT64 value);
void config_write_string(HANDLE file_handle,const ES_UTF8 *name,const wchar_t *value);
BOOL config_read_string(config_ini_t *ini,const ES_UTF8 *name,wchar_buf_t *wcbuf);
int config_read_int(config_ini_t *ini,const ES_UTF8 *name,int default_value);
DWORD config_read_dword(config_ini_t *ini,const ES_UTF8 *name,DWORD default_value);
ES_UINT64 config_read_uint64(config_ini_t *ini,const ES_UTF8 *name,ES_UINT64 default_value);

BOOL config_ini_open(config_ini_t *ini,const wchar_t *filename,const ES_UTF8 *section);
void config_ini_close(config_ini_t *ini);
