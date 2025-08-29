
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

typedef struct column_s
{
	// order list.
	struct column_s *order_next;
	struct column_s *order_prev;
	
	// EVERYTHING3_PROPERTY_ID_*
	DWORD property_id;
	
}column_t;

int column_compare(const column_t *a,const void *property_id);
column_t *column_find(DWORD property_id);
void column_insert_order_at_end(column_t *column);
void column_insert_order_at_start(column_t *column);
void column_remove_order(column_t *column);
column_t *column_add(DWORD property_id);
void column_remove(DWORD property_id);
void column_clear_all(void);
void column_insert_after(column_t *column,column_t *insert_after_column);
void column_move_order_to_start(DWORD property_id);

extern pool_t *column_pool; // pool of column_t
extern array_t *column_array; // array of column_t
extern column_t *column_order_start; // column order
extern column_t *column_order_last;
