
enum 
{
	PROPERTY_FORMAT_NONE,
	PROPERTY_FORMAT_TEXT30, // 30 characters
	PROPERTY_FORMAT_TEXT47, // 47 characters
	PROPERTY_FORMAT_SIZE, // 123456789
	PROPERTY_FORMAT_EXTENSION, // 4 characters
	PROPERTY_FORMAT_DATE, // 2025-06-16 21:22:23
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
	PROPERTY_FORMAT_YESNO, // 30 characters
	PROPERTY_FORMAT_COUNT,
};

#define _PROPERTY_BASIC_MACRO(name,property_id)	+1

enum 
{ 
    PROPERTY_BASIC_COUNT = 0
    #include "property_basic_macro.h"
};

#undef _PROPERTY_BASIC_MACRO

typedef struct property_basic_name_to_id_s
{
	const char *name;
	WORD id;
	
}property_basic_name_to_id_t;

/*
typedef struct es_property_name_to_id_s
{
	const char *name;
	WORD id;
	
}es_property_name_to_id_t;
*/

BYTE property_get_format(DWORD property_id);
BOOL property_is_right_aligned(DWORD property_id);
void property_get_name(DWORD property_id,wchar_buf_t *out_wcbuf);

extern BYTE property_format_to_column_width[PROPERTY_FORMAT_COUNT];
extern BYTE property_format_to_right_align[PROPERTY_FORMAT_COUNT];
extern const property_basic_name_to_id_t property_basic_name_to_id_array[PROPERTY_BASIC_COUNT];

