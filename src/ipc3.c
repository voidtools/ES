
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
// IPC v3 (Everything 1.5)

#include "es.h"

#define _IPC3_STREAM_PIPE_MAX_BUF_SIZE						65536
#define _IPC3_STREAM_POOL_CHUNK_SIZE						65536
#define _IPC3_IOCTL_ALLOC_OUT_CHUNK_DATA(chunk)				((BYTE *)(((ipc3_ioctl_alloc_out_chunk_t *)(chunk)) + 1))
#define _IPC3_CONNECT_BUSY_TIMEOUT							30000

typedef struct ipc3_ioctl_alloc_out_chunk_s
{
	// next chunk in the list.
	struct ipc3_ioctl_alloc_out_chunk_s *next;

	// size in bytes of the following data.
	DWORD size;

	// data follows
	// BYTE data[size];
	
}ipc3_ioctl_alloc_out_chunk_t;

static void _ipc3_stream_pipe_seek_proc(ipc3_stream_t *stream,ES_UINT64 position_from_start);
static ES_UINT64 _ipc3_stream_pipe_tell_proc(ipc3_stream_t *stream);
static SIZE_T _ipc3_stream_pipe_read_proc(ipc3_stream_t *stream,void *buf,SIZE_T size);
static void _ipc3_stream_pipe_close_proc(ipc3_stream_t *stream);
static void _ipc3_stream_pool_seek_proc(ipc3_stream_t *stream,ES_UINT64 position_from_start);
static ES_UINT64 _ipc3_stream_pool_tell_proc(ipc3_stream_t *stream);
static SIZE_T _ipc3_stream_pool_read_proc(ipc3_stream_t *stream,void *buf,SIZE_T size);
static void _ipc3_stream_pool_close_proc(ipc3_stream_t *stream);

static ipc3_stream_vtbl_t _ipc3_stream_pipe_vtbl =
{
	_ipc3_stream_pipe_seek_proc,
	_ipc3_stream_pipe_tell_proc,
	_ipc3_stream_pipe_read_proc,
	_ipc3_stream_pipe_close_proc,
};

static ipc3_stream_vtbl_t _ipc3_stream_pool_vtbl =
{
	_ipc3_stream_pool_seek_proc,
	_ipc3_stream_pool_tell_proc,
	_ipc3_stream_pool_read_proc,
	_ipc3_stream_pool_close_proc,
};

#pragma pack (push,1)

typedef struct _ipc3_read_journal_s
{
	ES_UINT64 journal_id;
	ES_UINT64 change_id;
	DWORD flags;
	
}_ipc3_read_journal_t;

#pragma pack (pop)

const char *_ipc3_journal_item_type_lowercase_name_array[] = 
{
	"folder-create",
	"folder-delete",
	"folder-rename",
	"folder-move",
	"folder-modify",
	"file-create",
	"file-delete",
	"file-rename",
	"file-move",
	"file-modify",
};

#define _IPC3_JOURNAL_ITEM_TYPE_LOWERCASE_NAME_COUNT	(sizeof(_ipc3_journal_item_type_lowercase_name_array) / sizeof(const char *))

// write some data to a pipe handle.
// returns TRUE if all data is written to the pipe.
// Otherwise, returns FALSE.
BOOL ipc3_write_pipe_data(HANDLE pipe_handle,const void *in_data,SIZE_T in_size)
{
	const BYTE *p;
	SIZE_T run;
	
	p = in_data;
	run = in_size;
	
	while(run)
	{
		DWORD chunk_size;
		DWORD num_written;
		
		if (run <= 65536)
		{
			chunk_size = (DWORD)run;
		}
		else
		{
			chunk_size = 65536;
		}
		
		if (WriteFile(pipe_handle,p,chunk_size,&num_written,NULL))
		{
			if (num_written)
			{
				p += num_written;
				run -= num_written;
			}
			else
			{
				// pipe EOF
				return FALSE;
			}
		}
		else
		{
			debug_error_printf("pipe WriteFile failed %u\n",GetLastError());
		
			return FALSE;
		}
	}

	return TRUE;
}

// send some data to the IPC pipe.
// returns TRUE if successful.
// returns FALSE on error. Call GetLastError() for more information.
BOOL ipc3_write_pipe_message(HANDLE pipe_handle,DWORD code,const void *in_data,SIZE_T in_size)
{
	// make sure the in_size is sane.
	if (in_size <= ES_DWORD_MAX)
	{
		ipc3_message_t send_message;
		
		send_message.code = code;
		send_message.size = (DWORD)in_size;
		
		if (ipc3_write_pipe_data(pipe_handle,&send_message,sizeof(ipc3_message_t)))
		{
			if (ipc3_write_pipe_data(pipe_handle,in_data,(DWORD)in_size))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

// Read some data from the IPC Pipe.
void ipc3_stream_read_data(ipc3_stream_t *stream,void *data,SIZE_T size)
{
	SIZE_T numread;
	
	numread = stream->vtbl->read_proc(stream,data,size);

	// zero what we didn't read..
	os_zero_memory(((BYTE *)data) + numread,size - numread);
	
	if (numread != size)
	{
		// we expected the data so set is_error.
		stream->is_error = 1;
	}
}

// read len, allocate some buffer for len and read data.
void ipc3_stream_read_utf8_string(ipc3_stream_t *stream,utf8_buf_t *out_cbuf)
{
	SIZE_T len;
	
	len = ipc3_stream_read_len_vlq(stream);
	
	utf8_buf_grow_length(out_cbuf,len);
	
	ipc3_stream_read_data(stream,out_cbuf->buf,len);
	out_cbuf->buf[len] = 0;
}

// Like ipc3_stream_read_data, except we don't set is_error if we don't read all the data.
// returns the number of bytes read.
// check stream->is_error for any errors.
SIZE_T ipc3_stream_try_read_data(ipc3_stream_t *stream,void *data,SIZE_T size)
{
	return stream->vtbl->read_proc(stream,data,size);
}

// skip over some data in the pipe stream.
void ipc3_stream_skip(ipc3_stream_t *stream,SIZE_T size)
{
	BYTE buf[256];
	SIZE_T run;
	
	run = size;
	
	while(run)
	{
		SIZE_T read_size;
		
		read_size = run;
		if (read_size > 256)
		{	
			read_size = 256;
		}
		
		ipc3_stream_read_data(stream,buf,read_size);
		
		run -= read_size;
	}
}

// read a BYTE value from the pipe stream.
BYTE ipc3_stream_read_byte(ipc3_stream_t *stream)
{
	BYTE value;
	
	ipc3_stream_read_data(stream,&value,sizeof(BYTE));	
	
	return value;
}

// read a WORD value from the pipe stream.
WORD ipc3_stream_read_word(ipc3_stream_t *stream)
{
	WORD value;
	
	ipc3_stream_read_data(stream,&value,sizeof(WORD));	
	
	return value;
}

// read a DWORD value from the pipe stream.
DWORD ipc3_stream_read_dword(ipc3_stream_t *stream)
{
	DWORD value;
	
	ipc3_stream_read_data(stream,&value,sizeof(DWORD));	
	
	return value;
}

// read a UINT64 value from the pipe stream.
ES_UINT64 ipc3_stream_read_uint64(ipc3_stream_t *stream)
{
	ES_UINT64 value;
	
	ipc3_stream_read_data(stream,&value,sizeof(ES_UINT64));	
	
	return value;
}

// get a SIZE_T, where the size can differ to Everything.
// we set the error code if the value would overflow. (we are 32bit and Everything is 64bit and the value is > 0xffffffff)
SIZE_T ipc3_stream_read_size_t(ipc3_stream_t *stream)
{
	SIZE_T ret;
	
	ret = SIZE_MAX;
	
	if (stream->is_64bit)
	{
		ES_UINT64 uint64_value;
		
		uint64_value = ipc3_stream_read_uint64(stream);	
					
#if SIZE_MAX == ES_UINT64_MAX

		ret = uint64_value;

#elif SIZE_MAX == ES_DWORD_MAX

		if (uint64_value <= SIZE_MAX)
		{
			ret = (SIZE_T)uint64_value;
		}
		else
		{
			stream->is_error = 1;
		}
		
#else

		#error unknown SIZE_MAX
		
#endif		
	}
	else
	{
		ret = ipc3_stream_read_dword(stream);	
	}

	return ret;
}

// read a variable length quantity.
// Doesn't have to be too efficient as the data will follow immediately.
// Sets the error code if the length would overflow (32bit dll, 64bit Everything, len > 0xffffffff )
SIZE_T ipc3_stream_read_len_vlq(ipc3_stream_t *stream)
{
	BYTE byte_value;
	WORD word_value;
	DWORD dword_value;
	SIZE_T start;
	
	start = 0;

	// BYTE
	byte_value = ipc3_stream_read_byte(stream);
	
	if (byte_value < 0xff)
	{
		return byte_value;
	}

	// WORD
	start = safe_size_add(start,0xff);
	
	word_value = ipc3_stream_read_word(stream);
	
	if (word_value < 0xffff)
	{
		return safe_size_add(start,word_value);
	}	
	
	// DWORD
	start = safe_size_add(start,0xffff);

	dword_value = ipc3_stream_read_dword(stream);
	
	if (dword_value < 0xffffffff)
	{
		return safe_size_add(start,dword_value);
	}

#if SIZE_MAX == ES_UINT64_MAX

	{
		ES_UINT64 uint64_value;
		
		// UINT64
		start = safe_size_add(start,0xffffffff);

		uint64_value = ipc3_stream_read_uint64(stream);

		if (uint64_value < ES_UINT64_MAX)
		{
			return safe_size_add(start,uint64_value);
		}

		stream->is_error = 1;
		return ES_UINT64_MAX;
	}
	
#elif SIZE_MAX == ES_DWORD_MAX
		
	stream->is_error = 1;
	return ES_DWORD_MAX;

#else

	#error unknown SIZE_MAX

#endif
}

// Receive some data from the IPC pipe.
// returns TRUE if successful.
// returns FALSE on error. Call GetLastError() for more information.
BOOL ipc3_read_pipe(HANDLE pipe_handle,void *buf,SIZE_T buf_size)
{
	BYTE *recv_p;
	SIZE_T recv_run;
	
	recv_p = (BYTE *)buf;
	recv_run = buf_size;

	while(recv_run)
	{
		DWORD chunk_size;
		DWORD numread;
		
		if (recv_run <= 65536)
		{
			chunk_size = (DWORD)recv_run;
		}
		else
		{
			chunk_size = 65536;
		}
		
		if (ReadFile(pipe_handle,recv_p,chunk_size,&numread,NULL))
		{
			if (numread)
			{
				recv_p += numread;
				recv_run -= numread;
				
				// continue..
			}
			else
			{
				// pipe EOF
				return FALSE;
			}
		}
		else
		{
			debug_error_printf("pipe ReadFile failed %u\n",GetLastError());
			
			return FALSE;
		}
	}
	
	return TRUE;
}

// skip some data from the pipe.
BOOL ipc3_skip_pipe(HANDLE pipe_handle,SIZE_T buf_size)
{
	BYTE buf[256];
	SIZE_T run;
	
	run = buf_size;
	
	while(run)
	{
		SIZE_T read_size;
		
		read_size = run;
		if (read_size > 256)
		{
			read_size = 256;
		}
		
		if (!ipc3_read_pipe(pipe_handle,buf,read_size))
		{
			return 0;
		}
		
		run -= read_size;
	}
	
	return 1;
}

// connect to Everything pipe server.
// returns a pipe handle 
// returns INVALID_HANDLE_VALUE if not pipe servers are available.
// Sets last error to ERROR_PIPE_NOT_CONNECTED on failure.
// doesn't wait for the pipe server.
// use the ipc window to handle timeouts.
// Everything 1.5.0.1400 and later will create the ipc pipe server BEFORE the ipc window.
HANDLE ipc3_connect_pipe()
{
	wchar_buf_t pipe_name_wcbuf;
	wchar_buf_t window_class_wcbuf;
	HANDLE pipe_handle;
	DWORD tickstart;
	
	wchar_buf_init(&pipe_name_wcbuf);
	wchar_buf_init(&window_class_wcbuf);

	ipc3_get_pipe_name(&pipe_name_wcbuf);
	tickstart = GetTickCount();
		
retry:
		
	pipe_handle = CreateFile(pipe_name_wcbuf.buf,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);

	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		// connected
	}
	else
	{
		DWORD last_error;
		
		last_error = GetLastError();
		
		debug_error_printf("Connect pipe %S CreateFile failed %u\n",pipe_name_wcbuf.buf,last_error);
		
		if (last_error == ERROR_PIPE_BUSY)
		{
			DWORD elapsed;
			
			elapsed = GetTickCount() - tickstart;
			
			if (elapsed < _IPC3_CONNECT_BUSY_TIMEOUT)
			{
				DWORD wait_last_error;
				
				if (WaitNamedPipe(pipe_name_wcbuf.buf,_IPC3_CONNECT_BUSY_TIMEOUT - elapsed))
				{
					goto retry;
				}

				wait_last_error = GetLastError();
				
				debug_error_printf("Connect pipe %S WaitNamedPipe failed %u\n",pipe_name_wcbuf.buf,wait_last_error);
			}
		}
		
		SetLastError(ERROR_PIPE_NOT_CONNECTED);
	}

	wchar_buf_kill(&window_class_wcbuf);
	wchar_buf_kill(&pipe_name_wcbuf);
	
	return pipe_handle;
}

// perform an IOCTL on the Everything IPC3 pipe.
// command is one of the IPC3_COMMAND_* commands.
// input and output will depend on the command.
// see the command documentation for input and output specification.
//
// writes the input to the pipe and waits for a reply.
// stores the reply in the output buffer.
// returns TRUE if successful.
// Otherwise, returns FALSE on failure.
// Sets last error.
BOOL ipc3_ioctl(HANDLE pipe_handle,int command,const void *in_buf,SIZE_T in_size,void *out_buf,SIZE_T out_size,SIZE_T *out_numread)
{
	if (ipc3_write_pipe_message(pipe_handle,command,in_buf,in_size))
	{
		ipc3_message_t recv_header;
		BYTE *out_d;
		SIZE_T out_run;
		
		out_d = out_buf;
		out_run = out_size;
		
		for(;;)
		{
			int is_more;
			DWORD read_size;
			
			if (!ipc3_read_pipe(pipe_handle,&recv_header,sizeof(ipc3_message_t)))
			{
				break;
			}
			
			is_more = 0;
			
			if (recv_header.code == IPC3_RESPONSE_OK_MORE_DATA)
			{
				is_more = 1;
			}
			else
			if (recv_header.code == IPC3_RESPONSE_OK)
			{
			}
			else
			{
				break;
			}
			
			read_size = recv_header.size;
			
			if (out_run <= ES_DWORD_MAX)
			{
				if (read_size > (DWORD)out_run)
				{
					read_size = (DWORD)out_run;
				}
			}
				
			if (read_size)
			{
				if (!ipc3_read_pipe(pipe_handle,out_d,read_size))
				{
					break;
				}
			}
			
			// skip overflow.
			if (!ipc3_skip_pipe(pipe_handle,recv_header.size - read_size))
			{
				break;
			}
			
			out_d += read_size;
			out_run -= read_size;
			
			if (!is_more)
			{
				*out_numread = out_d - (BYTE *)out_buf;
				
				return TRUE;
			}
		}
	}
	else
	{
		SetLastError(ERROR_WRITE_FAULT);
	}
		
	return FALSE;
}

// same as ipc3_ioctl
// except, the out_size is expected.
// returns FALSE if out_size doesn't match numread.
// Sets last error.
BOOL ipc3_ioctl_expect_output_size(HANDLE pipe_handle,int command,const void *in_buf,SIZE_T in_size,void *out_buf,SIZE_T out_size)
{
	SIZE_T numread;
	
	if (ipc3_ioctl(pipe_handle,command,in_buf,in_size,out_buf,out_size,&numread))
	{
		if (out_size == numread)
		{
			return TRUE;
		}
		else
		{
			SetLastError(ERROR_READ_FAULT);
		}
	}
	
	return FALSE;
}

// same as ipc3_ioctl
// except, the out buffer is allocated.
// returns FALSE if there's not enought memory or a pipe read error.
BOOL ipc3_ioctl_alloc_out(HANDLE pipe_handle,int command,const void *in_buf,SIZE_T in_size,utf8_buf_t *out_cbuf)
{
	BOOL ret;
	ipc3_ioctl_alloc_out_chunk_t *chunk_start;
	ipc3_ioctl_alloc_out_chunk_t *chunk_last;
	SIZE_T total_chunk_size;

	ret = FALSE;
	chunk_start = NULL;
	chunk_last = NULL;
	total_chunk_size = 0;

	if (ipc3_write_pipe_message(pipe_handle,command,in_buf,in_size))
	{
		ipc3_message_t recv_header;
		
		for(;;)
		{
			int is_more;
			BYTE *out_data;
			
			if (!ipc3_read_pipe(pipe_handle,&recv_header,sizeof(ipc3_message_t)))
			{
				break;
			}
			
			is_more = 0;
			
			if (recv_header.code == IPC3_RESPONSE_OK_MORE_DATA)
			{
				ipc3_ioctl_alloc_out_chunk_t *chunk;
				
				chunk = mem_try_alloc(safe_size_add(sizeof(ipc3_ioctl_alloc_out_chunk_t),recv_header.size));
				if (!chunk)
				{
					break;
				}
				
				chunk->size = recv_header.size;
				
				out_data = _IPC3_IOCTL_ALLOC_OUT_CHUNK_DATA(chunk);
				
				if (chunk_start)
				{
					chunk_last->next = chunk;
				}
				else
				{
					chunk_start = chunk;
				}
				
				chunk->next = NULL;
				chunk_last = chunk;
				total_chunk_size += recv_header.size;
			
				is_more = 1;
			}
			else
			if (recv_header.code == IPC3_RESPONSE_OK)
			{
				SIZE_T total_size;
				
				total_size = safe_size_add(total_chunk_size,recv_header.size);
				
				// we know the total size.
				if (!utf8_buf_try_grow_size(out_cbuf,total_size))
				{
					break;
				}
				
				out_cbuf->length_in_bytes = total_size;
				
				// copy chunks...
				{
					ipc3_ioctl_alloc_out_chunk_t *chunk;
					BYTE *out_d;
					
					chunk = chunk_start;
					out_d = out_cbuf->buf;
					
					while(chunk)
					{
						os_copy_memory(out_d,_IPC3_IOCTL_ALLOC_OUT_CHUNK_DATA(chunk),chunk->size);
						
						out_d += chunk->size;
						
						chunk = chunk->next;
					}

					// write the last pipe data straight to the buffer..
					out_data = out_d;
				}
			}
			else
			{
				break;
			}
			
			if (recv_header.size)
			{
				if (!ipc3_read_pipe(pipe_handle,out_data,recv_header.size))
				{
					break;
				}
			}
			
			if (!is_more)
			{
				ret = TRUE;
				break;
			}
		}
	}
	
	// free chunks.
	{
		ipc3_ioctl_alloc_out_chunk_t *chunk;
		
		chunk = chunk_start;
		while(chunk)
		{
			ipc3_ioctl_alloc_out_chunk_t *next_chunk;
			
			next_chunk = chunk->next;
		
			mem_free(chunk);
			
			chunk = next_chunk;
		}
	}
	
	return ret;
}

// get the pipe name and store it in out_wcbuf.
// appends the instance name to the pipe name if one is defined.
void ipc3_get_pipe_name(wchar_buf_t *out_wcbuf)
{
	wchar_buf_copy_utf8_string(out_wcbuf,"\\\\.\\PIPE\\Everything IPC");

	if (es_instance_name_wcbuf->length_in_wchars)
	{
		wchar_buf_cat_utf8_string(out_wcbuf," (");
		wchar_buf_cat_wchar_string_n(out_wcbuf,es_instance_name_wcbuf->buf,es_instance_name_wcbuf->length_in_wchars);
		wchar_buf_cat_utf8_string(out_wcbuf,")");
	}
}

// initialize a pipe stream
// the pipe_handle is just used a reference and should not be closed until the stream is closed.
void ipc3_stream_pipe_init(ipc3_stream_pipe_t *stream,HANDLE pipe_handle)
{
	stream->base.vtbl = &_ipc3_stream_pipe_vtbl;
	stream->base.is_error = 0;
	stream->base.is_64bit = 0;
	stream->base.response_code = 0;
	
	stream->pipe_handle = pipe_handle;
	stream->buf = NULL;
	stream->p = NULL;
	stream->avail = 0;
	stream->is_last = 0;
	stream->is_eof = 0;
	stream->pipe_avail = 0;
	stream->buf_size = 0;
	stream->pipe_totread = 0;
}

// close a stream.
void ipc3_stream_close(ipc3_stream_t *stream)				
{
	stream->vtbl->close_proc(stream);
}

// seek to a specific position in the pipe.
// which is not possible, so just set the error flag
static void _ipc3_stream_pipe_seek_proc(ipc3_stream_t *stream,ES_UINT64 position_from_start)
{
	// not supported.
	stream->is_error = 1;
}

// get the current position of the pipe stream.
// we maintain the total read from the pipe so we just take of whats available in the buffer.
static ES_UINT64 _ipc3_stream_pipe_tell_proc(ipc3_stream_t *stream)
{	
	return ((ipc3_stream_pipe_t *)stream)->pipe_totread	- ((ipc3_stream_pipe_t *)stream)->avail;
}

// read from a pipe stream.
// tries to read from a buffer first.
// if the buffer is empty, read a chunk from the pipe into the buffer and try again.
// returns the number of bytes read.
// MUST set is_error on any errors.
static SIZE_T _ipc3_stream_pipe_read_proc(ipc3_stream_t *stream,void *buf,SIZE_T size)
{
	BYTE *d;
	SIZE_T run;
	
	// read past the end of file.
	if (stream->is_error)
	{
		return 0;
	}
	
	// read past the end of file.
	if (((ipc3_stream_pipe_t *)stream)->is_eof)
	{
		stream->is_error = 1;
		
		return 0;
	}
	
	d = (BYTE *)buf;
	run = size;
	
	while(run)
	{
		SIZE_T chunk_size;
		
		if (!((ipc3_stream_pipe_t *)stream)->avail)
		{
			for(;;)
			{
				// is there some data to read from the pipe?
				if (((ipc3_stream_pipe_t *)stream)->pipe_avail)
				{
					DWORD read_size;
					
					// fill the buffer with data from the pipe.
					if (!((ipc3_stream_pipe_t *)stream)->buf)
					{
						DWORD alloc_size;
						
						alloc_size = _IPC3_STREAM_PIPE_MAX_BUF_SIZE;
						
						if (((ipc3_stream_pipe_t *)stream)->is_last)
						{
							// don't allocate more than we need.
							if (alloc_size > ((ipc3_stream_pipe_t *)stream)->pipe_avail)
							{
								alloc_size = ((ipc3_stream_pipe_t *)stream)->pipe_avail;
							}
						}
						
						((ipc3_stream_pipe_t *)stream)->buf_size = alloc_size;
						((ipc3_stream_pipe_t *)stream)->buf = mem_try_alloc(alloc_size);
						if (!((ipc3_stream_pipe_t *)stream)->buf)
						{
							stream->is_error = 1;
							
							return d - (BYTE *)buf;
						}
					}
					
					read_size = ((ipc3_stream_pipe_t *)stream)->pipe_avail;
					if (read_size > ((ipc3_stream_pipe_t *)stream)->buf_size)
					{
						read_size = ((ipc3_stream_pipe_t *)stream)->buf_size;
					}
							
					if (!ipc3_read_pipe(((ipc3_stream_pipe_t *)stream)->pipe_handle,((ipc3_stream_pipe_t *)stream)->buf,read_size))
					{
						stream->is_error = 1;

						return d - (BYTE *)buf;
					}
					
					((ipc3_stream_pipe_t *)stream)->pipe_totread += read_size;
					((ipc3_stream_pipe_t *)stream)->pipe_avail -= read_size;
					
					((ipc3_stream_pipe_t *)stream)->p = ((ipc3_stream_pipe_t *)stream)->buf;
					((ipc3_stream_pipe_t *)stream)->avail = read_size;
					
					break;
				}
				else
				{
					ipc3_message_t recv_header;
					
					if (((ipc3_stream_pipe_t *)stream)->is_last)
					{
						// do not read again.
						((ipc3_stream_pipe_t *)stream)->is_eof = 1;

						return d - (BYTE *)buf;
					}
					
					// read the header.
					if (!ipc3_read_pipe(((ipc3_stream_pipe_t *)stream)->pipe_handle,&recv_header,sizeof(ipc3_message_t)))
					{
						// read header failed.
						stream->is_error = 1;
						
						return d - (BYTE *)buf;
					}
					
					if (recv_header.code == IPC3_RESPONSE_OK_MORE_DATA)
					{
						// expect more headers and data..
					}
					else
					if (recv_header.code == IPC3_RESPONSE_OK)
					{
						((ipc3_stream_pipe_t *)stream)->is_last = 1;
					}
					else
					{
						stream->is_error = 1;
						stream->response_code = recv_header.code;
						
						return d - (BYTE *)buf;
					}
					
					((ipc3_stream_pipe_t *)stream)->pipe_avail = recv_header.size;
				}
			}
		}
		
		// stream->avail can be zero if we received a zero-sized data message.
		
		chunk_size = run;
		if (chunk_size > ((ipc3_stream_pipe_t *)stream)->avail)
		{
			chunk_size = ((ipc3_stream_pipe_t *)stream)->avail;
		}
		
		os_copy_memory(d,((ipc3_stream_pipe_t *)stream)->p,chunk_size);
		
		((ipc3_stream_pipe_t *)stream)->p += chunk_size;
		((ipc3_stream_pipe_t *)stream)->avail -= chunk_size;
		
		d += chunk_size;
		run -= chunk_size;
	}
	
	return d - (BYTE *)buf; 
}

// close a pipe stream.
static void _ipc3_stream_pipe_close_proc(ipc3_stream_t *stream)
{
	if (((ipc3_stream_pipe_t *)stream)->buf)
	{
		mem_free(((ipc3_stream_pipe_t *)stream)->buf);
	}
}

// Look up a property by property id and check if it is indexed.
// returns TRUE if indexed.
// Otherwise returns FALSE.
// Returns FALSE if unknown or if an error occurs.
BOOL ipc3_is_property_indexed(HANDLE pipe_handle,DWORD property_id)
{
	DWORD value;
	
	if (ipc3_ioctl_expect_output_size(pipe_handle,IPC3_COMMAND_IS_PROPERTY_INDEXED,&property_id,sizeof(DWORD),&value,sizeof(DWORD)))
	{
		if (value)
		{
			return TRUE;
		}
	}
	
	return FALSE;
}

// initialize a result list.
void ipc3_result_list_init(ipc3_result_list_t *result_list,ipc3_stream_t *stream)
{
	utf8_buf_init(&result_list->property_request_cbuf);

	result_list->stream = stream;
	result_list->total_result_size = ES_UINT64_MAX;

	result_list->index_to_stream_offset_array = NULL;
	result_list->index_to_stream_offset_valid_count = 0;

	// get reply flags
	result_list->valid_flags = ipc3_stream_read_dword(stream);

	// setup x64 stream.
	if (result_list->valid_flags & IPC3_SEARCH_FLAG_64BIT)
	{
		stream->is_64bit = 1;
	}

	// result counts
	result_list->folder_result_count = ipc3_stream_read_size_t(stream);
	result_list->file_result_count = ipc3_stream_read_size_t(stream);
	
	// total size.
	if (result_list->valid_flags & IPC3_SEARCH_FLAG_TOTAL_SIZE)
	{
		result_list->total_result_size = ipc3_stream_read_uint64(stream);
	}
	
	// viewport
	result_list->viewport_offset = ipc3_stream_read_size_t(stream);
	result_list->viewport_count = ipc3_stream_read_size_t(stream);
	
	// sort
	result_list->sort_count = ipc3_stream_read_len_vlq(stream);
	
	if (result_list->sort_count)
	{
		SIZE_T sort_run;
		
		sort_run = result_list->sort_count;
		
		while(sort_run)
		{
			DWORD sort_property_id;
			DWORD sort_flags;
			
			sort_property_id = ipc3_stream_read_dword(stream);
			sort_flags = ipc3_stream_read_dword(stream);
			
			sort_run--;
		}
	}

	// property requests
	result_list->property_request_count = ipc3_stream_read_len_vlq(stream);
	
	if (result_list->property_request_count)
	{
		SIZE_T property_request_size;
		
		property_request_size = safe_size_mul(sizeof(ipc3_result_list_property_request_t),result_list->property_request_count);
		
		utf8_buf_grow_size(&result_list->property_request_cbuf,property_request_size);
		
		{
			SIZE_T property_request_run;
			ipc3_result_list_property_request_t *property_request_d;
			
			property_request_run = result_list->property_request_count;
			property_request_d = (ipc3_result_list_property_request_t *)result_list->property_request_cbuf.buf;
			
			while(property_request_run)
			{
				property_request_d->property_id = ipc3_stream_read_dword(stream);
				property_request_d->flags = ipc3_stream_read_dword(stream);
				property_request_d->value_type = ipc3_stream_read_byte(stream);
				
				property_request_d++;
				property_request_run--;
			}
		}
	}
}

// destroy a result list.
void ipc3_result_list_kill(ipc3_result_list_t *result_list)
{
	if (result_list->index_to_stream_offset_array)
	{
		mem_free(result_list->index_to_stream_offset_array);
	}
	
	utf8_buf_kill(&result_list->property_request_cbuf);
}

// setup the vtbl object, save a reference to the source stream (doesn't hold ownership)
// set the current position to 0.
// the source stream MUST exist while this stream exists.
void ipc3_stream_pool_init(ipc3_stream_pool_t *stream,ipc3_stream_t *source_stream)
{
	stream->base.vtbl = &_ipc3_stream_pool_vtbl;
	stream->base.is_error = 0;
	stream->base.is_64bit = source_stream->is_64bit;
	stream->base.response_code = 0;
	
	stream->source_stream = source_stream;
	
	stream->chunk_cur = SIZE_MAX;
	stream->p = NULL;
	stream->avail = 0;
	stream->is_last = 0;
	
	array_init(&((ipc3_stream_pool_t *)stream)->chunk_array);
}

// seek to a specific location in a pool stream.
// we can calculate the chunk index from the position as we have fixed sized chunks.
// add the remainder to the current chunk.
static void _ipc3_stream_pool_seek_proc(ipc3_stream_t *stream,ES_UINT64 position_from_start)
{
	SIZE_T chunk_index;
	
	chunk_index = safe_size_from_uint64(position_from_start / _IPC3_STREAM_POOL_CHUNK_SIZE);

	if (chunk_index < ((ipc3_stream_pool_t *)stream)->chunk_array.count)
	{
		BYTE *chunk;
		SIZE_T chunk_offset;
		
		chunk = ((ipc3_stream_pool_t *)stream)->chunk_array.indexes[chunk_index];
		
		((ipc3_stream_pool_t *)stream)->chunk_cur = chunk_index;
		
		chunk_offset = (SIZE_T)(position_from_start % _IPC3_STREAM_POOL_CHUNK_SIZE);
		
		((ipc3_stream_pool_t *)stream)->p = chunk + chunk_offset;
		((ipc3_stream_pool_t *)stream)->avail = _IPC3_STREAM_POOL_CHUNK_SIZE - chunk_offset;
	}
	else
	if (position_from_start == 0)
	{
		// we haven't read anything yet if 
		// ((ipc3_stream_pool_t *)stream)->chunk_array.count
		// is zero.
	}
	else
	{
		// bad ref.
		stream->is_error = 1;
	}
}

// get the current position of a pool stream
// calculated from the current chunk index and the current position in the current chunk.
static ES_UINT64 _ipc3_stream_pool_tell_proc(ipc3_stream_t *stream)
{	
	BYTE *chunk;
	
	if (((ipc3_stream_pool_t *)stream)->chunk_cur == SIZE_MAX)
	{
		// haven't read anything yet..
		return 0;
	}
	
	chunk = ((ipc3_stream_pool_t *)stream)->chunk_array.indexes[((ipc3_stream_pool_t *)stream)->chunk_cur];
	
	return (((ipc3_stream_pool_t *)stream)->chunk_cur * _IPC3_STREAM_POOL_CHUNK_SIZE) + ((ipc3_stream_pool_t *)stream)->p - chunk;
}

// the read proc for a pool stream.
// tries to read from the current chunk if data is available.
// if not, it setups the next chunk and tries again..
// returns the number of bytes read.
// MUST set is_error on any errors.
static SIZE_T _ipc3_stream_pool_read_proc(ipc3_stream_t *stream,void *buf,SIZE_T size)
{
	BYTE *d;
	SIZE_T run;
	
	if (stream->is_error)
	{
		return 0;
	}
	
	d = (BYTE *)buf;
	run = size;
	
	while(run)
	{
		SIZE_T copy_size;
		
		if (!((ipc3_stream_pool_t *)stream)->avail)
		{
			// is there some data to read from the memory?
			if ((((ipc3_stream_pool_t *)stream)->chunk_cur == SIZE_MAX) || (((ipc3_stream_pool_t *)stream)->chunk_cur + 1 >= ((ipc3_stream_pool_t *)stream)->chunk_array.count))
			{
				BYTE *chunk;
				SIZE_T numread;
				
				// already got last chunk.
				if (((ipc3_stream_pool_t *)stream)->is_last)
				{
					stream->is_error = 1;
					
					return d - (BYTE *)buf;
				}
				
				// new chunk
				chunk = mem_try_alloc(65536);
				if (!chunk)
				{
					stream->is_error = 1;
					
					return d - (BYTE *)buf;
				}
				
				// add to our chunk list.
				array_insert(&((ipc3_stream_pool_t *)stream)->chunk_array,SIZE_MAX,chunk);
				
				// now actually try to read from the source stream, which might fail..
				numread = ipc3_stream_try_read_data(((ipc3_stream_pool_t *)stream)->source_stream,chunk,_IPC3_STREAM_POOL_CHUNK_SIZE);
				
				if (numread != _IPC3_STREAM_POOL_CHUNK_SIZE)
				{
					// read last chunk.
					((ipc3_stream_pool_t *)stream)->is_last = 1;
					
					((ipc3_stream_pool_t *)stream)->last_chunk_numread = numread;
				}
			}

			// next chunk..
			if (((ipc3_stream_pool_t *)stream)->chunk_cur == SIZE_MAX)
			{
				((ipc3_stream_pool_t *)stream)->chunk_cur = 0;
			}
			else
			{
				((ipc3_stream_pool_t *)stream)->chunk_cur++;
			}
			
			// setup current position
			((ipc3_stream_pool_t *)stream)->p = ((ipc3_stream_pool_t *)stream)->chunk_array.indexes[((ipc3_stream_pool_t *)stream)->chunk_cur];

			// setup available size.
			// if we are the last chunk use the last numread size.
			if ((((ipc3_stream_pool_t *)stream)->is_last) && (((ipc3_stream_pool_t *)stream)->chunk_cur == ((ipc3_stream_pool_t *)stream)->chunk_array.count - 1))
			{
				((ipc3_stream_pool_t *)stream)->avail = ((ipc3_stream_pool_t *)stream)->last_chunk_numread;
			}
			else
			{
				((ipc3_stream_pool_t *)stream)->avail = _IPC3_STREAM_POOL_CHUNK_SIZE;
			}
		}
		
		copy_size = run;
		if (copy_size > ((ipc3_stream_pool_t *)stream)->avail)
		{
			copy_size = ((ipc3_stream_pool_t *)stream)->avail;
		}
		
		os_copy_memory(d,((ipc3_stream_pool_t *)stream)->p,copy_size);
		
		((ipc3_stream_pool_t *)stream)->p += copy_size;
		((ipc3_stream_pool_t *)stream)->avail -= copy_size;
		
		d += copy_size;
		run -= copy_size;
	}
					
	return d - (BYTE *)buf;
}

// close a pool stream.
// releases chunks and kills the array
static void _ipc3_stream_pool_close_proc(ipc3_stream_t *stream)
{
	// free chunks.
	
	{
		SIZE_T chunk_index;
		
		for(chunk_index=0;chunk_index<((ipc3_stream_pool_t *)stream)->chunk_array.count;chunk_index++)
		{
			mem_free(((ipc3_stream_pool_t *)stream)->chunk_array.indexes[chunk_index]);
		}
	}
	
	array_kill(&((ipc3_stream_pool_t *)stream)->chunk_array);
}

// seek to the specified position from the start of the pool stream.
void ipc3_stream_seek(ipc3_stream_t *stream,ES_UINT64 position_from_start)
{
	stream->vtbl->seek_proc(stream,position_from_start);
}

// get the current position of the stream.
// needed so we can seek later.
ES_UINT64 ipc3_stream_tell(ipc3_stream_t *stream)
{	
	return stream->vtbl->tell_proc(stream);
}

// get the stream offset from the start index
// cache the result.
void ipc3_result_list_seek_to_offset_from_index(ipc3_result_list_t *result_list,SIZE_T start_index)
{
	SIZE_T last_index;
	SIZE_T last_offset;
	SIZE_T property_request_count;
	ipc3_result_list_property_request_t *property_request_array;
	ipc3_stream_t *stream;

	stream = result_list->stream;

	if (start_index >= result_list->viewport_count)
	{
		// bad request index.
		es_fatal(ES_ERROR_IPC_ERROR);
	}

	if (start_index < result_list->index_to_stream_offset_valid_count)
	{
		// we have already seen this item.
		ipc3_stream_seek(stream,result_list->index_to_stream_offset_array[start_index]);
		
		return;
	}
	
	property_request_count = result_list->property_request_count;
	property_request_array = (ipc3_result_list_property_request_t *)result_list->property_request_cbuf.buf;

	// search from the last index and offset.
	last_index = 0;
	last_offset = 0;
	
	if (result_list->index_to_stream_offset_valid_count)
	{
		last_index = result_list->index_to_stream_offset_valid_count - 1;
		last_offset = result_list->index_to_stream_offset_array[result_list->index_to_stream_offset_valid_count - 1];
	}
	else
	{
		result_list->index_to_stream_offset_array[0] = 0;
		result_list->index_to_stream_offset_valid_count++;
	}
	
	ipc3_stream_seek(stream,last_offset);
	
	// find the offset and cache it..
	for(;;)
	{
		BYTE item_flags;
		SIZE_T property_request_run;
		const ipc3_result_list_property_request_t *property_request_p;
		
		if (last_index == start_index)
		{
			// found it.
			break;
		}
		
		// item flags.
		item_flags = ipc3_stream_read_byte(stream);
		
		// read stream.
		// skip over properties.
		property_request_run = property_request_count;
		property_request_p = property_request_array;
		
		// read remaining pipe data.
		// we shouldn't any remaining data.
		// this should really be an error.
		while(property_request_run)
		{
			// skip it.
			if (property_request_p->flags & (IPC3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT|IPC3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT))
			{
				SIZE_T len;
				
				len = ipc3_stream_read_len_vlq(stream);
				
				ipc3_stream_skip(stream,len);
			}
			else
			{
				// add to total item size.
				switch(property_request_p->value_type)
				{
					case IPC3_PROPERTY_VALUE_TYPE_PSTRING: 
					case IPC3_PROPERTY_VALUE_TYPE_PSTRING_MULTISTRING: 
					case IPC3_PROPERTY_VALUE_TYPE_PSTRING_STRING_REFERENCE:
					case IPC3_PROPERTY_VALUE_TYPE_PSTRING_FOLDER_REFERENCE:
					case IPC3_PROPERTY_VALUE_TYPE_PSTRING_FILE_OR_FOLDER_REFERENCE:

						{
							SIZE_T len;
							
							len = ipc3_stream_read_len_vlq(stream);
							
							ipc3_stream_skip(stream,len);
						}

						break;

					case IPC3_PROPERTY_VALUE_TYPE_BYTE:
					case IPC3_PROPERTY_VALUE_TYPE_BYTE_GET_TEXT:

						ipc3_stream_skip(stream,sizeof(BYTE));

						break;

					case IPC3_PROPERTY_VALUE_TYPE_WORD:
					case IPC3_PROPERTY_VALUE_TYPE_WORD_GET_TEXT:

						ipc3_stream_skip(stream,sizeof(WORD));
						
						break;

					case IPC3_PROPERTY_VALUE_TYPE_DWORD: 
					case IPC3_PROPERTY_VALUE_TYPE_DWORD_FIXED_Q1K: 
					case IPC3_PROPERTY_VALUE_TYPE_DWORD_GET_TEXT: 

						ipc3_stream_skip(stream,sizeof(DWORD));
						
						break;
						
					case IPC3_PROPERTY_VALUE_TYPE_UINT64: 

						ipc3_stream_skip(stream,sizeof(ES_UINT64));
						
						break;
						
					case IPC3_PROPERTY_VALUE_TYPE_UINT128: 

						ipc3_stream_skip(stream,sizeof(EVERYTHING3_UINT128));
						
						break;
						
					case IPC3_PROPERTY_VALUE_TYPE_DIMENSIONS: 

						ipc3_stream_skip(stream,sizeof(EVERYTHING3_DIMENSIONS));
						
						break;
						
					case IPC3_PROPERTY_VALUE_TYPE_SIZE_T:
					
						ipc3_stream_read_size_t(stream);
						break;
						
					case IPC3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1K: 
					case IPC3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1M: 

						ipc3_stream_skip(stream,sizeof(__int32));
						
						break;

					case IPC3_PROPERTY_VALUE_TYPE_BLOB8:

						{
							BYTE len;
							
							len = ipc3_stream_read_byte(stream);
							
							ipc3_stream_skip(stream,len);
						}

						break;

					case IPC3_PROPERTY_VALUE_TYPE_BLOB16:

						{
							WORD len;
							
							len = ipc3_stream_read_word(stream);
							
							ipc3_stream_skip(stream,len);
						}

						break;

				}
			}
			
			property_request_p++;
			property_request_run--;
		}
		
		last_index++;
		last_offset = (SIZE_T)ipc3_stream_tell(stream);
		
		// cache it.
		result_list->index_to_stream_offset_array[result_list->index_to_stream_offset_valid_count] = last_offset;
		result_list->index_to_stream_offset_valid_count++;
	}
}


// fill in a buffer with a VLQ value and progress the buffer pointer.
// buf can be NULL to calculate the length.
BYTE *ipc3_copy_len_vlq(BYTE *buf,SIZE_T value)
{
	BYTE *d;
	
	d = buf;
	
	if (value < 0xff)
	{
		if (buf)
		{
			*d++ = (BYTE)value;
		}
		else
		{
			d++;
		}
		
		return d;
	}
	
	value -= 0xff;

	if (buf)
	{
		*d++ = 0xff;
	}
	else
	{
		d++;
	}
	
	if (value < 0xffff)
	{
		if (buf)
		{
			*d++ = (BYTE)(value);
			*d++ = (BYTE)(value >> 8);
		}
		else
		{
			d += 2;
		}
		
		return d;
	}
	
	value -= 0xffff;

	if (buf)
	{
		*d++ = 0xff;
		*d++ = 0xff;
	}
	else
	{
		d += 2;
	}
	
	if (value < 0xffffffff)
	{
		if (buf)
		{
			*d++ = (BYTE)(value);
			*d++ = (BYTE)(value >> 8);
			*d++ = (BYTE)(value >> 16);
			*d++ = (BYTE)(value >> 24);
		}
		else
		{
			d += 4;
		}
		
		return d;
	}
	
	value -= 0xffffffff;

	if (buf)
	{
		*d++ = 0xff;
		*d++ = 0xff;
		*d++ = 0xff;
		*d++ = 0xff;
	}
	else
	{
		d += 4;
	}
	
	if (buf)
	{
		*d++ = (BYTE)((ES_UINT64)value);
		*d++ = (BYTE)(((ES_UINT64)value) >> 8);
		*d++ = (BYTE)(((ES_UINT64)value) >> 16);
		*d++ = (BYTE)(((ES_UINT64)value) >> 24);
		*d++ = (BYTE)(((ES_UINT64)value) >> 32);
		*d++ = (BYTE)(((ES_UINT64)value) >> 40);
		*d++ = (BYTE)(((ES_UINT64)value) >> 48);
		*d++ = (BYTE)(((ES_UINT64)value) >> 56);
	}
	else
	{
		d += 8;
	}
	
	// value cannot be larger than or equal to 0xffffffffffffffff
#if SIZE_MAX == ES_UINT64_MAX

#elif SIZE_MAX == ES_DWORD_MAX

#else
	#error unknown SIZE_MAX
#endif

	return d;
}

// returns EVERYTHING3_INVALID_PROPERTY_ID if not found.
DWORD ipc3_find_property(const wchar_t *search)
{
	DWORD ret;
	HANDLE pipe_handle;

	ret = EVERYTHING3_INVALID_PROPERTY_ID;
	
	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		utf8_buf_t search_cbuf;
		DWORD ipc3_property_id;
		
		utf8_buf_init(&search_cbuf);

		utf8_buf_copy_wchar_string(&search_cbuf,search);
		
		if (ipc3_ioctl_expect_output_size(pipe_handle,IPC3_COMMAND_FIND_PROPERTY_FROM_NAME,search_cbuf.buf,search_cbuf.length_in_bytes,&ipc3_property_id,sizeof(DWORD)))
		{
			// found the property id.
			ret = ipc3_property_id;
		}

		utf8_buf_kill(&search_cbuf);

		CloseHandle(pipe_handle);
	}

	return ret;
}

// out_cbuf is NOT NULL terminated.
BOOL ipc3_get_property_canonical_name(DWORD property_id,utf8_buf_t *out_cbuf)
{
	BOOL ret;
	HANDLE pipe_handle;

	ret = FALSE;
	
	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		if (ipc3_ioctl_alloc_out(pipe_handle,IPC3_COMMAND_GET_PROPERTY_CANONICAL_NAME,&property_id,sizeof(DWORD),out_cbuf))
		{
			// found the property id.
			ret = TRUE;
		}

		CloseHandle(pipe_handle);
	}

	return ret;
}

// out_cbuf is NOT NULL terminated.
BOOL ipc3_get_property_localized_name(DWORD property_id,utf8_buf_t *out_cbuf)
{
	BOOL ret;
	HANDLE pipe_handle;

	ret = FALSE;
	
	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		if (ipc3_ioctl_alloc_out(pipe_handle,IPC3_COMMAND_GET_PROPERTY_NAME,&property_id,sizeof(DWORD),out_cbuf))
		{
			// found the property id.
			ret = TRUE;
		}

		CloseHandle(pipe_handle);
	}

	return ret;
}

BOOL ipc3_is_property_right_aligned(DWORD property_id)
{
	BOOL ret;
	HANDLE pipe_handle;

	ret = FALSE;
	
	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		DWORD is_right_aligned;
		
		if (ipc3_ioctl_expect_output_size(pipe_handle,IPC3_COMMAND_IS_PROPERTY_RIGHT_ALIGNED,&property_id,sizeof(DWORD),&is_right_aligned,sizeof(DWORD)))
		{
			// found the property id.
			if (is_right_aligned)
			{
				ret = TRUE;
			}
			else
			{
				SetLastError(0);
			}
		}

		CloseHandle(pipe_handle);
	}

	return ret;
}

BOOL ipc3_is_property_sort_descending(DWORD property_id)
{
	BOOL ret;
	HANDLE pipe_handle;

	ret = FALSE;
	
	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		DWORD is_sort_descending;
		
		if (ipc3_ioctl_expect_output_size(pipe_handle,IPC3_COMMAND_IS_PROPERTY_SORT_DESCENDING,&property_id,sizeof(DWORD),&is_sort_descending,sizeof(DWORD)))
		{
			// found the property id.
			if (is_sort_descending)
			{
				ret = TRUE;
			}
			else
			{
				SetLastError(0);
			}
		}

		CloseHandle(pipe_handle);
	}

	return ret;
}

int ipc3_get_property_default_width(DWORD property_id)
{
	int ret;
	HANDLE pipe_handle;

	ret = 0;
	
	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		DWORD default_width;
		
		if (ipc3_ioctl_expect_output_size(pipe_handle,IPC3_COMMAND_GET_PROPERTY_DEFAULT_WIDTH,&property_id,sizeof(DWORD),&default_width,sizeof(DWORD)))
		{
			// found the property id.
			if (default_width)
			{
				ret = (int)default_width;
			}
			else
			{
				SetLastError(0);
			}
		}

		CloseHandle(pipe_handle);
	}

	return ret;
}

// sets last error on failure.
BOOL ipc3_get_journal_info(ipc3_journal_info_t *out_journal_info)
{
	BOOL ret;
	HANDLE pipe_handle;

	ret = FALSE;

	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		if (ipc3_ioctl_expect_output_size(pipe_handle,IPC3_COMMAND_GET_JOURNAL_INFO,NULL,0,out_journal_info,sizeof(ipc3_journal_info_t)))
		{
			ret = TRUE;
		}
	
		CloseHandle(pipe_handle);
	}
	
	return ret;
}

// this doesn't normally return.
// returns if callback_proc returns FALSE or if you exit Everything.
// sets last error on failure.
// ERROR_FILE_NOT_FOUND == journal id/change id not found.
// ERROR_PIPE_NOT_CONNECTED == no ipc
// ERROR_WRITE_FAULT == cannot write to ipc.
// ERROR_INVALID_FUNCTION == bad response
// ERROR_CANCELLED == callback_proc returned FALSE.
BOOL ipc3_read_journal(ES_UINT64 journal_id,ES_UINT64 change_id,DWORD flags,void *user_data,BOOL (*callback_proc)(void *user_data,_ipc3_journal_change_t *change))
{
	HANDLE pipe_handle;

	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		_ipc3_read_journal_t read_journal;
		
		read_journal.journal_id = journal_id;
		read_journal.change_id = change_id;
		read_journal.flags = flags;
		
		if (ipc3_write_pipe_message(pipe_handle,IPC3_COMMAND_READ_JOURNAL,&read_journal,sizeof(_ipc3_read_journal_t)))
		{
			ipc3_stream_pipe_t pipe_stream;
			utf8_buf_t old_path_cbuf;
			utf8_buf_t old_name_cbuf;
			utf8_buf_t new_path_cbuf;
			utf8_buf_t new_name_cbuf;
			_ipc3_journal_change_t change;
				
			utf8_buf_init(&old_path_cbuf);
			utf8_buf_init(&old_name_cbuf);
			utf8_buf_init(&new_path_cbuf);
			utf8_buf_init(&new_name_cbuf);

			change.journal_id = journal_id;			

			ipc3_stream_pipe_init(&pipe_stream,pipe_handle);
			
			for(;;)
			{
				change.type = ipc3_stream_read_byte((ipc3_stream_t *)&pipe_stream);

				if (flags & IPC3_READ_JOURNAL_FLAG_CHANGE_ID)
				{
					change.change_id = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
				}
				else
				{
					change.change_id = ES_UINT64_MAX;
				}
				
				if (flags & IPC3_READ_JOURNAL_FLAG_TIMESTAMP)
				{
					change.timestamp = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
				}
				else
				{
					change.timestamp = ES_UINT64_MAX;
				}
				
				if (flags & IPC3_READ_JOURNAL_FLAG_SOURCE_TIMESTAMP)
				{
					change.source_timestamp = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
				}
				else
				{
					change.source_timestamp = ES_UINT64_MAX;
				}
				
				if (flags & IPC3_READ_JOURNAL_FLAG_OLD_PARENT_DATE_MODIFIED)
				{
					change.old_parent_date_modified = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
				}
				else
				{
					change.old_parent_date_modified = ES_UINT64_MAX;
				}
				
				if (flags & IPC3_READ_JOURNAL_FLAG_OLD_PATH)
				{
					ipc3_stream_read_utf8_string((ipc3_stream_t *)&pipe_stream,&old_path_cbuf);
				}
				else
				{
					utf8_buf_empty(&old_path_cbuf);
				}
				
				if (flags & IPC3_READ_JOURNAL_FLAG_OLD_NAME)
				{
					ipc3_stream_read_utf8_string((ipc3_stream_t *)&pipe_stream,&old_name_cbuf);
				}
				else
				{
					utf8_buf_empty(&old_name_cbuf);
				}
				
				change.old_path = old_path_cbuf.buf;
				change.old_path_len = old_path_cbuf.length_in_bytes;
				change.old_name = old_name_cbuf.buf;
				change.old_name_len = old_name_cbuf.length_in_bytes;
				
				change.size = ES_UINT64_MAX;
				change.date_created = ES_UINT64_MAX;
				change.date_modified = ES_UINT64_MAX;
				change.date_accessed = ES_UINT64_MAX;
				change.attributes = INVALID_FILE_ATTRIBUTES;
				change.new_parent_date_modified = ES_UINT64_MAX;
				change.new_path = "";
				change.new_path_len = 0;
				change.new_name = "";
				change.new_name_len = 0;
						
				switch(change.type)
				{
					case IPC3_JOURNAL_ITEM_TYPE_FILE_CREATE:
					case IPC3_JOURNAL_ITEM_TYPE_FILE_MODIFY:
					case IPC3_JOURNAL_ITEM_TYPE_FILE_RENAME:
					case IPC3_JOURNAL_ITEM_TYPE_FILE_MOVE:
					case IPC3_JOURNAL_ITEM_TYPE_FOLDER_CREATE:
					case IPC3_JOURNAL_ITEM_TYPE_FOLDER_MODIFY:
					case IPC3_JOURNAL_ITEM_TYPE_FOLDER_RENAME:
					case IPC3_JOURNAL_ITEM_TYPE_FOLDER_MOVE:
				
						// size
						switch(change.type)
						{
							case IPC3_JOURNAL_ITEM_TYPE_FILE_CREATE:
							case IPC3_JOURNAL_ITEM_TYPE_FILE_MODIFY:
							case IPC3_JOURNAL_ITEM_TYPE_FILE_RENAME:
							case IPC3_JOURNAL_ITEM_TYPE_FILE_MOVE:
						
								if (flags & IPC3_READ_JOURNAL_FLAG_SIZE)
								{
									change.size = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
								}

								break;
						}

						// date created
						if (flags & IPC3_READ_JOURNAL_FLAG_DATE_CREATED)
						{
							change.date_created = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
						}
						
						// date modified
						if (flags & IPC3_READ_JOURNAL_FLAG_DATE_CREATED)
						{
							change.date_modified = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
						}

						// date accessed.
						if (flags & IPC3_READ_JOURNAL_FLAG_DATE_ACCESSED)
						{
							change.date_accessed = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
						}

						// attributes
						if (flags & IPC3_READ_JOURNAL_FLAG_ATTRIBUTES)
						{
							change.attributes = ipc3_stream_read_dword((ipc3_stream_t *)&pipe_stream);
						}
					
						break;
				}
							
				switch(change.type)
				{
					case IPC3_JOURNAL_ITEM_TYPE_FILE_RENAME:
					case IPC3_JOURNAL_ITEM_TYPE_FOLDER_RENAME:

						// new name
						if (flags & IPC3_READ_JOURNAL_FLAG_NEW_NAME)
						{
							ipc3_stream_read_utf8_string((ipc3_stream_t *)&pipe_stream,&new_name_cbuf);

							change.new_name = new_name_cbuf.buf;
							change.new_name_len = new_name_cbuf.length_in_bytes;
						}
							
						break;

					case IPC3_JOURNAL_ITEM_TYPE_FILE_MOVE:
					case IPC3_JOURNAL_ITEM_TYPE_FOLDER_MOVE:

						// new parent date modified					
						if (flags & IPC3_READ_JOURNAL_FLAG_NEW_PARENT_DATE_MODIFIED)
						{
							change.new_parent_date_modified = ipc3_stream_read_uint64((ipc3_stream_t *)&pipe_stream);
						}
					
						// new parent
						if (flags & IPC3_READ_JOURNAL_FLAG_NEW_PATH)
						{
							ipc3_stream_read_utf8_string((ipc3_stream_t *)&pipe_stream,&new_path_cbuf);

							change.new_path = new_path_cbuf.buf;
							change.new_path_len = new_path_cbuf.length_in_bytes;
						}
						
						// new name
						if (flags & IPC3_READ_JOURNAL_FLAG_NEW_NAME)
						{
							ipc3_stream_read_utf8_string((ipc3_stream_t *)&pipe_stream,&new_name_cbuf);

							change.new_name = new_name_cbuf.buf;
							change.new_name_len = new_name_cbuf.length_in_bytes;
						}
							
						break;
				}
				
				if (pipe_stream.base.is_error)
				{
					if (pipe_stream.base.response_code == IPC3_RESPONSE_ERROR_NOT_FOUND)
					{
						SetLastError(ERROR_FILE_NOT_FOUND);
					}
					else
					{
						SetLastError(ERROR_INVALID_FUNCTION);
					}
					
					break;
				}
				
				if (!callback_proc(user_data,&change))
				{
					SetLastError(ERROR_CANCELLED);
					
					break;
				}
			}

			utf8_buf_kill(&new_name_cbuf);
			utf8_buf_kill(&new_path_cbuf);
			utf8_buf_kill(&old_name_cbuf);
			utf8_buf_kill(&old_path_cbuf);

			ipc3_stream_close((ipc3_stream_t *)&pipe_stream);
		}
		else
		{
			SetLastError(ERROR_WRITE_FAULT);
		}

		CloseHandle(pipe_handle);
	}

	// should never be reached unless there's an error.	
	return FALSE;
}

BOOL ipc3_journal_action_is_folder(int action)
{
	switch(action)
	{
		case IPC3_JOURNAL_ITEM_TYPE_FOLDER_CREATE:
		case IPC3_JOURNAL_ITEM_TYPE_FOLDER_DELETE:
		case IPC3_JOURNAL_ITEM_TYPE_FOLDER_RENAME:
		case IPC3_JOURNAL_ITEM_TYPE_FOLDER_MOVE:
		case IPC3_JOURNAL_ITEM_TYPE_FOLDER_MODIFY:
			return TRUE;
	}
	
	return FALSE;
}

int ipc3_journal_item_type_from_name(const wchar_t *name)
{
	int i;
	
	for(i=0;i<_IPC3_JOURNAL_ITEM_TYPE_LOWERCASE_NAME_COUNT;i++)
	{
		const wchar_t *p1;
		const char *p2;
		
		p1 = name;
		p2 = _ipc3_journal_item_type_lowercase_name_array[i];
		
		for(;;)
		{
			int c1;
			int c2;
			
			if (!*p2)
			{
				if (!*p1)
				{
					return i + 1;
				}
				
				// next item.
				break;
			}
			
			c2 = *p2;
			p2++;
			
			if (c2 == '-')
			{
				if ((*p1 == '-') || (*p1 == ' ') || (*p1 == '_') || (*p1 == '.'))
				{
					p1++;
				}

				continue;
			}
			
			c1 = unicode_ascii_to_lower(*p1);
			
			if (c1 != c2)
			{	
				// next item.
				break;
			}

			p1++;
		}
	}
	
	return 0;
}

