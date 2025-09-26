
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

enum 
{
	PROPERTY_FORMAT_NONE,
	PROPERTY_FORMAT_TEXT8, // 8 characters
	PROPERTY_FORMAT_TEXT10, // 10 characters
	PROPERTY_FORMAT_TEXT12, // 12 characters (8.3)
	PROPERTY_FORMAT_TEXT16, // 16 characters
	PROPERTY_FORMAT_TEXT24, // 24 characters
	PROPERTY_FORMAT_TEXT30, // 30 characters
	PROPERTY_FORMAT_TEXT32, // 32 characters
	PROPERTY_FORMAT_TEXT47, // 47 characters
	PROPERTY_FORMAT_TEXT48, // 48 characters
	PROPERTY_FORMAT_TEXT64, // 64 characters
	PROPERTY_FORMAT_SIZE, // 123456789
	PROPERTY_FORMAT_VOLUME_SIZE, // 999,999,999,999,999
	PROPERTY_FORMAT_EXTENSION, // 4 characters
	PROPERTY_FORMAT_FILETIME, // 2025-06-16 21:22:23
	PROPERTY_FORMAT_TIME, // 21:22:23
	PROPERTY_FORMAT_DATE, // 2025-06-16
	PROPERTY_FORMAT_ATTRIBUTES, // RASH
	PROPERTY_FORMAT_NOGROUPING_NUMBER1, // 1
	PROPERTY_FORMAT_NOGROUPING_NUMBER2, // 12
	PROPERTY_FORMAT_NOGROUPING_NUMBER3, // 123
	PROPERTY_FORMAT_NOGROUPING_NUMBER4, // 1234 (no comma)
	PROPERTY_FORMAT_NOGROUPING_NUMBER5, // 12345 (no comma)
	PROPERTY_FORMAT_GROUPING_NUMBER2, // 12
	PROPERTY_FORMAT_GROUPING_NUMBER3, // 123
	PROPERTY_FORMAT_GROUPING_NUMBER4, // 1,234
	PROPERTY_FORMAT_GROUPING_NUMBER5, // 12,345
	PROPERTY_FORMAT_GROUPING_NUMBER6, // 123,456
	PROPERTY_FORMAT_GROUPING_NUMBER7, // 1,234,567
	PROPERTY_FORMAT_KHZ, // 44.1 Khz
	PROPERTY_FORMAT_KBPS, // 9999 kbps
	PROPERTY_FORMAT_RATING, // *****
	PROPERTY_FORMAT_HEX_NUMBER8, // 0xdeadbeef
	PROPERTY_FORMAT_HEX_NUMBER16, // 0xdeadbeefdeadbeef
	PROPERTY_FORMAT_HEX_NUMBER32, // 0xdeadbeefdeadbeefdeadbeefdeadbeef
	PROPERTY_FORMAT_DIMENSIONS, // 123x456
	PROPERTY_FORMAT_F_STOP, // f/9.99
	PROPERTY_FORMAT_EXPOSURE_TIME, // 1/350 sec
	PROPERTY_FORMAT_ISO_SPEED, // ISO-9999
	PROPERTY_FORMAT_EXPOSURE_BIAS, // +1.234 step
	PROPERTY_FORMAT_FOCAL_LENGTH, // 999.999 mm
	PROPERTY_FORMAT_SUBJECT_DISTANCE, // 999.999 m
	PROPERTY_FORMAT_BCPS, // 9.999 bcps
	PROPERTY_FORMAT_35MM_FOCAL_LENGTH, // 99999 mm
	PROPERTY_FORMAT_ALTITUDE, // 9999.123456 m
	PROPERTY_FORMAT_SEC, // 99.999 sec
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
	PROPERTY_FORMAT_FORMATTED_TEXT8, // formatted 8 characters
	PROPERTY_FORMAT_FORMATTED_TEXT12, // formatted 12 characters
	PROPERTY_FORMAT_FORMATTED_TEXT16, // formatted 16 characters
	PROPERTY_FORMAT_FORMATTED_TEXT24, // formatted 30 characters
	PROPERTY_FORMAT_FORMATTED_TEXT32, // formatted 30 characters
	PROPERTY_FORMAT_YESNO, // Yes/No
	PROPERTY_FORMAT_PERCENT, // 100%
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
void property_get_localized_name(DWORD property_id,wchar_buf_t *out_wcbuf);
DWORD property_id_from_old_column_id(int i);
DWORD property_find(const wchar_t *s,int allow_property_system);
BOOL property_get_default_sort_ascending(DWORD property_id);
int property_get_default_width(DWORD property_id);
void property_cache_unknown_information(DWORD property_id);
void property_load_read_journal_names(void);
