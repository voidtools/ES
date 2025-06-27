
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
				return FALSE;
			}
		}
		else
		{
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
	BYTE *d;
	SIZE_T run;
	
	d = (BYTE *)data;
	run = size;
	
	while(run)
	{
		SIZE_T chunk_size;
		
		if (!stream->avail)
		{
			ipc3_message_t recv_header;
			
			if (stream->got_last)
			{
				// read passed EOF
				os_zero_memory(d,run);
				
				stream->is_error = 1;
			
				return;
			}
			
			if (!ipc3_read_pipe(stream->pipe_handle,&recv_header,sizeof(ipc3_message_t)))
			{
				// read header failed.
				os_zero_memory(d,run);
				
				stream->is_error = 1;
			
				return;
			}

			// last chunk?
			if (recv_header.code == IPC3_RESPONSE_OK_MORE_DATA)
			{
			}
			else
			if (recv_header.code == IPC3_RESPONSE_OK)
			{
				stream->got_last = 1;
			}
			else
			{
				os_zero_memory(d,run);
				
				stream->is_error = 1;
				
				return;
			}
			
			if (recv_header.size)			
			{
//TODO: dont reallocate.
				if (stream->buf)
				{
					mem_free(stream->buf);
				}
			
				stream->buf = mem_alloc(recv_header.size);
				if (!stream->buf)
				{
					os_zero_memory(d,run);
					
					stream->is_error = 1;
					
					break;
				}
						
				if (!ipc3_read_pipe(stream->pipe_handle,stream->buf,recv_header.size))
				{
					os_zero_memory(d,run);
					
					stream->is_error = 1;

					break;
				}
				
				stream->p = stream->buf;
				stream->avail = recv_header.size;
			}
		}
		
		// stream->avail can be zero if we received a zero-sized data message.
		
		chunk_size = run;
		if (chunk_size > stream->avail)
		{
			chunk_size = stream->avail;
		}
		
		CopyMemory(d,stream->p,chunk_size);
		
		stream->p += chunk_size;
		stream->avail -= chunk_size;
		
		d += chunk_size;
		run -= chunk_size;
	}
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
					
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64

		ret = uint64_value;

#elif SIZE_MAX == 0xFFFFFFFF

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

#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64

	{
		ES_UINT64 uint64_value;
		
		// UINT64
		start = safe_size_add(start,0xffffffff);

		uint64_value = ipc3_stream_read_uint64(stream);

		if (uint64_value < 0xFFFFFFFFFFFFFFFFUI64)
		{
			return safe_size_add(start,uint64_value);
		}

		stream->is_error = 1;
		return ES_UINT64_MAX;
	}
	
#elif SIZE_MAX == 0xffffffffui32
		
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
				return FALSE;
			}
		}
		else
		{
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

HANDLE ipc3_connect_pipe(void)
{
	wchar_buf_t pipe_name_wcbuf;
	HANDLE pipe_handle;
	
	wchar_buf_init(&pipe_name_wcbuf);

	ipc3_get_pipe_name(&pipe_name_wcbuf);
		
	pipe_handle = CreateFile(pipe_name_wcbuf.buf,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,0);
	
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
BOOL ipc3_pipe_ioctl(HANDLE pipe_handle,int command,const void *in_buf,SIZE_T in_size,void *out_buf,SIZE_T out_size,SIZE_T *out_numread)
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
	
	return FALSE;
}

// get the instance name and store it in wcbuf.
// instance_name must be non-NULL.
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

void ipc3_stream_init(ipc3_stream_t *stream,HANDLE pipe_handle)
{
	stream->pipe_handle = pipe_handle;
	stream->buf = NULL;
	stream->p = NULL;
	stream->avail = 0;
	stream->is_error = 0;
	stream->got_last = 0;
	stream->is_64bit = 0;
}

void ipc3_stream_kill(ipc3_stream_t *stream)				
{
	if (stream->buf)
	{
		mem_free(stream->buf);
	}
}
