#pragma once 
#include <stdint.h>
#include <stddef.h>
#include "fat.h"

typedef struct {
    // for now VFS stream == FAT stream
    fat_file_t fat;
} vfs_file_t;

typedef struct {
    uint8_t  is_dir;
    uint32_t size;
} vfs_stat_t;

int vfs_mount_root(void);


int vfs_ls(const char* path);
int vfs_cat(const char* path);


int vfs_read(const char* path, void* out_buf, uint32_t max_bytes, uint32_t* out_size);

int vfs_open(const char* path, vfs_file_t* out);
int vfs_read_file(vfs_file_t* f, void* out_buf, uint32_t bytes, uint32_t* out_read);
int vfs_seek(vfs_file_t* f, uint32_t new_pos);
int vfs_close(vfs_file_t* f); // no-op for now
int vfs_create(const char* path);
int vfs_unlink(const char* path);
int vfs_mkdir(const char* path);
int vfs_rmdir(const char* path);
int vfs_rename(const char* old_path, const char* new_path);
int vfs_move(const char* old_path, const char* new_path);
int vfs_write_file(const char* path, const void* data, uint32_t size);
int vfs_append_file(const char* path, const void* data, uint32_t size);


int vfs_read_at(vfs_file_t* f, uint32_t off, void* out_buf, uint32_t bytes, uint32_t* out_read);
int vfs_read_all(const char* path, void* out_buf, uint32_t max_bytes, uint32_t* out_size);

const char* vfs_strerror(int err);
int vfs_stat(const char* path, vfs_stat_t* st);
void test();