
enum 
{
	PROPERTY_FORMAT_NONE,
	PROPERTY_FORMAT_TEXT30, // 30 characters
	PROPERTY_FORMAT_TEXT47, // 47 characters
	PROPERTY_FORMAT_SIZE, // 123456789
	PROPERTY_FORMAT_EXTENSION, // 4 characters
	PROPERTY_FORMAT_FILETIME, // 2025-06-16 21:22:23
	PROPERTY_FORMAT_ATTRIBUTES, // RASH
	PROPERTY_FORMAT_NUMBER1, // 1
	PROPERTY_FORMAT_NUMBER2, // 12
	PROPERTY_FORMAT_NUMBER3, // 123
	PROPERTY_FORMAT_NUMBER4, // 1234 (no comma)
	PROPERTY_FORMAT_NUMBER5, // 12345 (no comma)
	PROPERTY_FORMAT_NUMBER6, // 12,345
	PROPERTY_FORMAT_NUMBER7, // 123,456
	PROPERTY_FORMAT_NUMBER,  // 123,456,789
	PROPERTY_FORMAT_HEX_NUMBER, // 0xdeadbeef
	PROPERTY_FORMAT_DIMENSIONS, // 123x456
	PROPERTY_FORMAT_FIXED_Q1K, // 0.123
	PROPERTY_FORMAT_FIXED_Q1M, // 0.123456
	PROPERTY_FORMAT_DURATION, // 1:23:45
	PROPERTY_FORMAT_DATA1, // data
	PROPERTY_FORMAT_DATA2, // data
	PROPERTY_FORMAT_DATA4, // crc
	PROPERTY_FORMAT_DATA8, // crc64
	PROPERTY_FORMAT_DATA16, // md5
	PROPERTY_FORMAT_DATA20, // sha1
	PROPERTY_FORMAT_DATA32, // sha256
	PROPERTY_FORMAT_DATA48, // sha384
	PROPERTY_FORMAT_DATA64, // sha512
	PROPERTY_FORMAT_DATA128, // data
	PROPERTY_FORMAT_DATA256, // data
	PROPERTY_FORMAT_DATA512, // data
	PROPERTY_FORMAT_YESNO, // 3 characters
	PROPERTY_FORMAT_COUNT,
};

// property name (or alias)
// and proeprty ID
typedef struct property_name_to_id_s
{
	const char *name;
	WORD id;
	
}property_name_to_id_t;

BYTE property_get_format(DWORD property_id);
BOOL property_is_right_aligned(DWORD property_id);
void property_get_canonical_name(DWORD property_id,wchar_buf_t *out_wcbuf);
DWORD property_id_from_old_column_id(int i);
DWORD property_find(const wchar_t *s);
BOOL property_get_default_sort_ascending(DWORD property_id);
int proerty_get_default_width(DWORD property_id);

