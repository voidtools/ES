
// create a new filelist

#ifdef __cplusplus
extern "C" {
#endif

int filelist_create(const utf8_t *filename,const utf8_t *path,int no_folders,int no_files,int no_subfolders,const utf8_t *include_only_folders,const utf8_t *exclude_folders,const utf8_t *include_only_files,const utf8_t *exclude_files);

#ifdef __cplusplus
}
#endif

