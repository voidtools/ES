
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
// Unicode functions

#include "es.h"

// ASCII only white space.
BOOL unicode_is_ascii_ws(int c)
{
	if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))
	{
		return TRUE;
	}
	
	return FALSE;
}

// convert ASCII character to lowercase.
int unicode_ascii_to_lower(int c)
{
	if ((c >= 'A') && (c <= 'Z'))
	{
		return c - 'A' + 'a';
	}
	
	return c;
}

// get the hex character for the specified value (0-15)
int unicode_hex_char(int value)
{
	if ((value >= 0) && (value < 10))
	{
		return value + '0';
	}

	return value - 10 + 'A';
}