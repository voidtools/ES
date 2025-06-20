int config_get_filename(int is_temp,wchar_buf_t *wcbuf);
void config_write_int(HANDLE file_handle,const char *name,int value);
void config_write_string(HANDLE file_handle,const char *name,const wchar_t *value);
int config_read_string(const char *name,const char *filename,wchar_buf_t *wcbuf);
int config_read_int(const char *filename,const char *name,int default_value);
