
// TODO
/*
#include "es.h"

#define _FILELIST_ITEM_FILENAME(filelist_item)		((ES_UTF8 *)(((_filelist_item_t *)(filelist_item)) + 1))

#define _FILELIST_MAX_DEPTH		64
	
#pragma pack (push,1)

// an editor item.
typedef struct _filelist_item_s
{
	struct _filelist_item_s *next;
	
	ES_UINT64 size;
	ES_UINT64 date_created;
	ES_UINT64 date_modified;
	DWORD attributes;
	BYTE flags;

	// NULL terminated filename follows.
	
}_filelist_item_t;

#pragma pack (pop)

// a list of filenames.
typedef struct _filelist_s
{
	pool_t data_pool;
	_filelist_item_t *item_start;
	_filelist_item_t *item_last;
//	filename_filter_t include_only_folders_filename_filter;
//	filename_filter_t exclude_folders_filename_filter;
//	filename_filter_t include_only_files_filename_filter;
//	filename_filter_t exclude_files_filename_filter;
	int no_folders;
	int no_files;
	int no_subfolders;
		
}_filelist_t;

static void _filelist_add_folder_from_path_recurse(_filelist_t *e,const wchar_t *path,uintptr_t path_len,uintptr_t depth);
static void _filelist_add_item(_filelist_t *e,const ES_UTF8 *filename,ES_UINT64 size,ES_UINT64 date_created,ES_UINT64 date_modified);
static int _filelist_startwith(const wchar_t *s,const wchar_t *substring);

int filelist_create(const wchar_t *filename,const wchar_t *path,int folder_append_path_separator,int no_folders,int no_files,int no_subfolders,const wchar_t *include_only_folders,const wchar_t *exclude_folders,const wchar_t *include_only_files,const wchar_t *exclude_files)
{
	int ret;
	_filelist_t filelist;
	
	ret = 0;
	os_zero_memory(&filelist,sizeof(_filelist_t));
	
	pool_init(&filelist.data_pool);

//	filename_filter_init(&filelist.include_only_folders_filename_filter);
//	filename_filter_init(&filelist.exclude_folders_filename_filter);
//	filename_filter_init(&filelist.include_only_files_filename_filter);
//	filename_filter_init(&filelist.exclude_files_filename_filter);
	
//	filename_filter_compile(&filelist.include_only_folders_filename_filter,include_only_folders ? include_only_folders : "",1);
//	filename_filter_compile(&filelist.exclude_folders_filename_filter,exclude_folders ? exclude_folders : "",1);
//	filename_filter_compile(&filelist.include_only_files_filename_filter,include_only_files ? include_only_files : "",0);
//	filename_filter_compile(&filelist.exclude_files_filename_filter,exclude_files ? exclude_files : "",0);

	filelist.no_folders = no_folders;
	filelist.no_files = no_files;
	filelist.no_subfolders = no_subfolders;

	
	// recurse path
	
	{
		utf8_buf_t item_cbuf;
		utf8_buf_t path_cbuf;
		const wchar_t *path_p;

		utf8_buf_init(&item_cbuf);
		utf8_buf_init(&path_cbuf);
		
		path_p = path;
		
		while(*path_p)
		{
			path_p = utf8_string_parse_c_item(path_p,&item_cbuf);
			if (!path_p)
			{
				break;
			}
			
			// uppercase drive letter
			if ((item_cbuf.buf[0] >= 'a') && (item_cbuf.buf[0] <= 'z') && (item_cbuf.buf[1] == ':'))
			{
				item_cbuf.buf[0] = item_cbuf.buf[0] - 'a' + 'A';
			}
			
			utf8_buf_copy_no_trailing_path_separator(&path_cbuf,item_cbuf.buf);
			
			utf8_buf_fix_path_separators(&path_cbuf);
			
			// add this path.
			// this makes it easy to update roots.
			if (!no_folders)
			{
				fileinfo_fd_t fd;

				os_get_fd(path_cbuf.buf,&fd);
				
				_filelist_add_item(&filelist,path_cbuf.buf,&fd);
			}

			_filelist_add_folder_from_path_recurse(&filelist,path_cbuf.buf,path_cbuf.len,0);
		}
		
		utf8_buf_kill(&path_cbuf);
		utf8_buf_kill(&item_cbuf);
	}

	// save
	{
		output_stream_t f;

		// create the file
		if (output_stream_create_file(&f,filename))
		{
			utf8_buf_t path_cbuf;
			uintptr_t path_len;
			
			output_stream_write_utf8_bom(&f);
			
			utf8_buf_init(&path_cbuf);
			path_len = 0;
			
			utf8_buf_copy_utf8_string(&path_cbuf,filename);
			
			// check filename prefix.
			{
				wchar_t *p;
				wchar_t *last;

				p = path_cbuf.buf;
				last = 0;
				
				while(*p)
				{
					if (*p == '\\')
					{
						// include the backslash.
						path_len = (p - path_cbuf.buf + 1);
					}
					else
					if (*p == '/')
					{
						// include the backslash.
						path_len = (p - path_cbuf.buf + 1);
					}
					
					p++;
				}
				
				path_cbuf.buf[path_len] = 0;
			}

			// make all files relative to filename.
			if (config_get_int_value(CONFIG_FILE_LIST_RELATIVE_PATHS))
			{
				_filelist_item_t *item;
				
				item = filelist.item_start;
				
				while(item)
				{
					// free item
					if (!_filelist_startwith(_FILELIST_ITEM_FILENAME(item),path_cbuf.buf))
					{
						path_len = 0;
						break;
					}
					
					item = item->next;
				}
			}
			else
			{
				path_len = 0;
			}

			// write header.
			output_stream_write_printf(&f,(const utf8_t *)"Filename,Size,Date Modified,Date Created,Attributes\r\n");

			{
				_filelist_item_t *item;
				
				item = filelist.item_start;
				
				while(item)
				{
					// filename
					if ((folder_append_path_separator) && (item->fd.attributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						output_stream_write_csv_path_with_trailing_path_separator(&f,_FILELIST_ITEM_FILENAME(item) + path_len);
					}
					else
					{
						output_stream_write_csv_string(&f,_FILELIST_ITEM_FILENAME(item) + path_len);
					}
					
					output_stream_write_byte(&f,',');
					
					// size
					if (item->fd.size != QWORD_MAX)
					{
						output_stream_write_printf(&f,(const utf8_t *)"%I64u",item->fd.size);
					}
					output_stream_write_byte(&f,',');
					
					// date modified
					if (item->fd.date_modified & QWORD_MAX)
					{
						output_stream_write_printf(&f,(const utf8_t *)"%I64u",item->fd.date_modified);
					}
					output_stream_write_byte(&f,',');
					
					// date created.
					if (item->fd.date_created & QWORD_MAX)
					{
						output_stream_write_printf(&f,(const utf8_t *)"%I64u",item->fd.date_created);
					}
					
					// attributes
					output_stream_write_printf(&f,(const utf8_t *)",%u\r\n",item->fd.attributes);

					item = item->next;
				}
			}

			// close file.		
			if (output_stream_close(&f))
			{
				ret = 1;
			}
			else
			{
				// failed to export
				ui_task_dialog_show(0,MB_OK|MB_ICONERROR,localization_get_string(LOCALIZATION_EVERYTHING),NULL,localization_get_string(LOCALIZATION_EXPORT_WRITE_FAILED),filename);
			}

			utf8_buf_kill(&path_cbuf);
		}
		else
		{
			// Message
			ui_task_dialog_show(0,MB_OK|MB_ICONERROR,localization_get_string(LOCALIZATION_EVERYTHING),NULL,localization_get_string(LOCALIZATION_EXPORT_CREATEFILE_FAILED),filename);
		}
	}

	filename_filter_kill(&filelist.include_only_folders_filename_filter);
	filename_filter_kill(&filelist.exclude_folders_filename_filter);
	filename_filter_kill(&filelist.include_only_files_filename_filter);
	filename_filter_kill(&filelist.exclude_files_filename_filter);
	
	// free items.
	pool_kill(&filelist.data_pool);
	
	return ret;
}

static int _filelist_startwith(const utf8_t *s,const utf8_t *substring)
{
	const utf8_t *sp;
	const utf8_t *ssp;
	
	sp = s;
	ssp = substring;
	
	while(*ssp)
	{
		if (*sp != *ssp) return 0;

		sp++;
		ssp++;
	}
	
	return 1;
}

static void _filelist_add_item(_filelist_t *filelist,const utf8_t *filename,fileinfo_fd_t *fd)
{
	_filelist_item_t *item;
	uintptr_t len;
	uintptr_t item_size;

	len = utf8_string_get_length_in_bytes(filename);

	// alloc
	item_size = sizeof(_filelist_item_t);
	item_size = safe_uintptr_add(item_size,len);
	item_size = safe_uintptr_add_one(item_size);
	
	item = pool_alloc(&filelist->data_pool,item_size);
	
	// init
	os_copy_memory(&item->fd,fd,sizeof(fileinfo_fd_t));

	os_copy_memory(item+1,filename,len+1);
	
	if (filelist->item_start)
	{
		filelist->item_last->next = item;
	}
	else
	{
		filelist->item_start = item;
	}
	
	filelist->item_last = item;
	item->next = 0;
}

// recursive.
static void _filelist_add_folder_from_path_recurse(_filelist_t *e,const utf8_t *path,uintptr_t path_len,uintptr_t depth)
{
	if (depth < _FILELIST_MAX_DEPTH)
	{
		os_find_t find_handle;
		fileinfo_fd_t fd;
		utf8_buf_t search_cbuf;
		utf8_buf_t filename_cbuf;
		
		utf8_buf_init(&search_cbuf);
		utf8_buf_init(&filename_cbuf);
		
		if (os_find_first_file(&find_handle,path,&filename_cbuf,&fd,0))
		{
			for(;;)
			{
				if (fd.attributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					// convert to utf8.
					utf8_buf_path_cat_filename_n(&search_cbuf,path,path_len,filename_cbuf.buf,filename_cbuf.len);
					
					if (!filename_filter_exec(&e->exclude_folders_filename_filter,search_cbuf.buf,search_cbuf.len,filename_cbuf.buf,filename_cbuf.len,1,0))
					{
						if (filename_filter_exec(&e->include_only_folders_filename_filter,search_cbuf.buf,search_cbuf.len,filename_cbuf.buf,filename_cbuf.len,1,1))
						{
							// add self
							if (!e->no_folders)
							{
								_filelist_add_item(e,search_cbuf.buf,&fd);
							}
							
							// add sub folders.				
							if (!e->no_subfolders)
							{
								_filelist_add_folder_from_path_recurse(e,search_cbuf.buf,search_cbuf.len,depth+1);
							}
						}
					}
				}
				else
				{
					// convert to utf8.
					utf8_buf_path_cat_filename(&search_cbuf,path,filename_cbuf.buf);

					if (!filename_filter_exec(&e->exclude_folders_filename_filter,search_cbuf.buf,search_cbuf.len,filename_cbuf.buf,filename_cbuf.len,0,0))
					{
						if (filename_filter_exec(&e->include_only_folders_filename_filter,search_cbuf.buf,search_cbuf.len,filename_cbuf.buf,filename_cbuf.len,0,1))
						{
							if (!filename_filter_exec(&e->exclude_files_filename_filter,search_cbuf.buf,search_cbuf.len,filename_cbuf.buf,filename_cbuf.len,0,0))
							{
								if (filename_filter_exec(&e->include_only_files_filename_filter,search_cbuf.buf,search_cbuf.len,filename_cbuf.buf,filename_cbuf.len,0,1))
								{
									if (!e->no_files)
									{
										// add it
										_filelist_add_item(e,search_cbuf.buf,&fd);
									}
								}
							}
						}
					}
				}
			
				if (!os_find_next_file(&find_handle,&filename_cbuf,&fd)) 
				{
					break;
				}
			}

			os_find_close(&find_handle);
		}

		utf8_buf_kill(&filename_cbuf);
		utf8_buf_kill(&search_cbuf);
	}
}

*/