#include "vfs.h"
#include "fat.h"
#include "../ide/ide.h"
#include "../../libs/memhelp.h"
#include "../serial.h"

static fat_info_t g_root;
static int g_root_mounted = 0;




int vfs_mount_root() {
    if (!ide_boot_disk) return -10;

    int r = fat_mount(&g_root, ide_boot_disk);
    if (r) return r;

    if(r == 0) g_root_mounted = 1;
    return r;
}

int vfs_ls(const char* path) {
    if (!g_root_mounted) return -20;
 fat_dir_entry_t ent;
 int r = fat_resolve_path(&g_root, path, &ent);
 if (r != 0) return r;

    if (!(ent.attr & 0x10)) return -21; // not a directory

    return fat_dir_list(&g_root, ent.first_cluster);

}

int vfs_cat(const char* path) {
    if (!g_root_mounted) return -20;

    vfs_file_t f;
    int r = vfs_open(path, &f);
    if (r) return r;

    char buf[128];
    for (;;) {
        uint32_t got = 0;
        r = vfs_read_file(&f, buf, sizeof(buf), &got);
        if (r) return r;
        if (got == 0) break; // EOF

        for (uint32_t i = 0; i < got; i++) {
            write_serial(buf[i]); // or serial_write_char
        }
    }

    write_serial('\n');
    return 0;
}



int vfs_open(const char* path, vfs_file_t* out) {
    if (!g_root_mounted) return -20;
    if (!out) return -21;
    return fat_open_path(&g_root, path, &out->fat);
}

int vfs_read_file(vfs_file_t* f, void* out_buf, uint32_t bytes, uint32_t* out_read) {
    if (!g_root_mounted) return -20;
    if (!f) return -21;
    return fat_read_file(&f->fat, out_buf, bytes, out_read);
}

int vfs_seek(vfs_file_t* f, uint32_t new_pos) {
    if (!g_root_mounted) return -20;
    if (!f) return -21;
    return fat_seek(&f->fat, new_pos);
}

int vfs_close(vfs_file_t* f) {
    (void)f;
    // FAT16 read-only stream holds no resources yet
    return 0;
}

int vfs_read_at(vfs_file_t* f, uint32_t off, void* out_buf, uint32_t bytes, uint32_t* out_read) {
    int r = vfs_seek(f, off);
    if (r) return r;
    return vfs_read_file(f, out_buf, bytes, out_read);
}

int vfs_read_all(const char* path, void* out_buf, uint32_t max_bytes, uint32_t* out_size) {
    if (!g_root_mounted) return -20;
    if (!path || !out_buf || !out_size) return -22;

    vfs_file_t f;
    int r = vfs_open(path, &f);
    if (r) return r;

    uint32_t total = 0;
    while (total < max_bytes) {
        uint32_t got = 0;
        r = vfs_read_file(&f, (uint8_t*)out_buf + total, max_bytes - total, &got);
        if (r) return r;
        if (got == 0) break; // EOF
        total += got;
    }

    *out_size = total;
    vfs_close(&f);
    return 0;
}

const char* vfs_strerror(int err) {
    switch (err) {
       case 0: return "OK";
        case -10: return "No boot disk";
        case -20: return "VFS not mounted";
        case -21: return "Invalid argument / not a directory";
        case -22: return "Invalid path";
        case -24: return "Is a directory";
        // FAT layer common ones (adjust to your real codes)
        case -200: return "Not found";
        case -900: return "Bad path";
        case -901: return "Path component not a directory";
        default: return "Unknown error";
    }
}

int vfs_stat(const char* path, vfs_stat_t* st) {
    if (!g_root_mounted) return -20;
    if (!path || !st) return -21;

    fat_dir_entry_t ent;
    int r = fat_resolve_path(&g_root, path, &ent);
    if (r) return r;

    st->is_dir = (ent.attr & 0x10) ? 1 : 0;
    st->size   = ent.file_size;
    return 0;
}


void test() {

 fat_test_dir_write(&g_root);

}

