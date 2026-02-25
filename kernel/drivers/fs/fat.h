#pragma once
#ifndef FAT_H
#define FAT_H
#include <stdint.h>
#include <stddef.h>
#include "../ide/ide.h"

#define FAT16_EOC_MIN 0xFFF8
#define FAT16_EOC     0xFFFF
#define FAT16_FREE    0x0000




typedef struct {
    ide_device_t* device;

    // BPB
    uint16_t bytes_per_sector;      // should be 512
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t sectors_per_fat;       // FAT16
    uint32_t total_sectors;

    // computed LBAs
    uint32_t fat_start_lba;
    uint32_t root_dir_start_lba;
    uint32_t data_start_lba;
    uint32_t root_dir_sectors;
} fat_info_t;

typedef struct {
    char     name_83[11];
    uint8_t  attr;
    uint16_t first_cluster;
    uint32_t file_size;
} fat_dir_entry_t;

typedef struct {
    fat_info_t* fs;

    uint16_t start_clusrter;
    uint16_t current_cluster;
    uint32_t size;
    uint32_t pos;

    uint32_t current_cluster_index; // index within the current cluster, for easier reading
    uint32_t current_cluster_sector; // which sector of the current cluster we are on
    uint32_t current_offset_within_sector; // offset within the current sector
} fat_file_t;

typedef struct {
  uint16_t dir_cluster; // cluster number of the directory being listed
  uint32_t lba;
  uint16_t off;
} fat_dirent_loc_t;

int fat_mount(fat_info_t* fs, ide_device_t* device);
int fat_root_list(fat_info_t* fs);
int fat_root_find(fat_info_t* fs, const char* name83, fat_dir_entry_t* out_entry);
uint16_t fat16_get_fat_entry(fat_info_t* fs, uint16_t cluster);
uint32_t fat16_cluster_to_lba(fat_info_t* fs, uint16_t cluster);
int fat16_read_file_root(fat_info_t* fs, const char* name83);
int fat_dir_list(fat_info_t* fs, uint16_t cluster);
int fat_dir_find(fat_info_t* fs, uint16_t cluster, const char* name83, fat_dir_entry_t* out_entry);
int fat_resolve_path(fat_info_t* fs, const char* path, fat_dir_entry_t* out_entry);
int fat_cat_by_cluster(fat_info_t* fs, uint16_t first_cluster, uint32_t file_size);
int fat_open_path(fat_info_t* fs, const char* path, fat_file_t* out_file);
int fat_read_file(fat_file_t* file, void* buf, uint32_t bytes_to_read, uint32_t* out_bytes_read);
int fat_seek(fat_file_t* file, uint32_t new_pos);
int fat16_set_fat_entry(fat_info_t* fs, uint16_t cluster, uint16_t value);
uint16_t fat_alloc_cluster(fat_info_t* fs);
int fat16_free_chain(fat_info_t* fs, uint16_t first_cluster);
int fat16_chain_get_length(fat_info_t* fs, uint16_t start, uint32_t n, uint16_t* out_cluster);
int fat_dir_find_loc(fat_info_t* fs, uint16_t dir_cluster, const char* name83, fat_dirent_loc_t* out_loc, fat_dir_entry_t* out_entry);
int fat_dir_find_free_slot(fat_info_t* fs, uint16_t dir_cluster, fat_dirent_loc_t* out_loc);
int fat_dir_write_raw_at(fat_info_t* fs, const fat_dirent_loc_t* loc, const fat_dir_entry_raw_t* raw);




#endif

