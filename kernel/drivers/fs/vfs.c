#include "vfs.h"
#include "fat.h"
#include "../ide/ide.h"
#include "../../libs/memhelp.h"
#include "../serial.h"

static fat_info_t g_root;
static int g_root_mounted = 0;

static int vfs_split_parent_leaf(const char* path, char* parent_out, uint32_t parent_cap, char* leaf_out, uint32_t leaf_cap) {
    if (!path || !parent_out || !leaf_out) return -1000;
    if (path[0] != '/') return -1001;

    // find last slash
    int last = -1;
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/') last = i;
    }

    if (last < 0) return -1002;

    // leaf must exist
    if (path[last + 1] == 0) return -1003; // trailing slash / empty leaf

    // copy parent
    if (last == 0) {
        // parent is root
        if (parent_cap < 2) return -1004;
        parent_out[0] = '/';
        parent_out[1] = 0;
    } else {
        if ((uint32_t)last + 1 > parent_cap) return -1004;
        for (int i = 0; i < last; i++) parent_out[i] = path[i];
        parent_out[last] = 0;
    }

    // copy leaf
    uint32_t o = 0;
    for (int i = last + 1; path[i] && o + 1 < leaf_cap; i++) {
        leaf_out[o++] = path[i];
    }
    leaf_out[o] = 0;

    if (leaf_out[0] == 0) return -1005;
    return 0;
}

static int vfs_resolve_dir_cluster(const char* dir_path, uint16_t* out_cluster) {
    if (!g_root_mounted) return -20;
    if (!dir_path || !out_cluster) return -1006;

    // root special case
    if (dir_path[0] == '/' && dir_path[1] == 0) {
        *out_cluster = 0;
        return 0;
    }

    fat_dir_entry_t ent;
    int r = fat_resolve_path(&g_root, dir_path, &ent);
    if (r) return r;

    if (!(ent.attr & 0x10)) return -1007; // not a directory

    *out_cluster = ent.first_cluster;
    return 0;
}


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

int vfs_create(const char* path) {
  if(!g_root_mounted) return -20;
    if(!path) return -21;

    char parent[256];
    char leaf[64];

    int r = vfs_split_parent_leaf(path, parent, sizeof(parent), leaf, sizeof(leaf));
    if (r) return r;

    uint16_t parent_cluster;
    r = vfs_resolve_dir_cluster(parent, &parent_cluster);
    if (r) return r;

    return fat_create_file(&g_root, parent_cluster, leaf);
}

int vfs_unlink(const char* path) {
    if(!g_root_mounted) return -20;
    if(!path) return -21;

    char parent[256];
    char leaf[64];

    int r = vfs_split_parent_leaf(path, parent, sizeof(parent), leaf, sizeof(leaf));
    if (r) return r;

    uint16_t parent_cluster;
    r = vfs_resolve_dir_cluster(parent, &parent_cluster);
    if (r) return r;

    return fat_unlink(&g_root, parent_cluster, leaf);
}

int vfs_mkdir(const char* path) {
    if(!g_root_mounted) return -20;
    if(!path) return -21;

    char parent[256];
    char leaf[64];

    int r = vfs_split_parent_leaf(path, parent, sizeof(parent), leaf, sizeof(leaf));
    if (r) return r;

    uint16_t parent_cluster;
    r = vfs_resolve_dir_cluster(parent, &parent_cluster);
    if (r) return r;

    return fat_mkdir(&g_root, parent_cluster, leaf);
}

int vfs_rmdir(const char* path) {
    if(!g_root_mounted) return -20;
    if(!path) return -21;

    char parent[256];
    char leaf[64];

    int r = vfs_split_parent_leaf(path, parent, sizeof(parent), leaf, sizeof(leaf));
    if (r) return r;

    uint16_t parent_cluster;
    r = vfs_resolve_dir_cluster(parent, &parent_cluster);
    if (r) return r;

    return fat_rmdir(&g_root, parent_cluster, leaf);
}

int vfs_rename(const char* old_path, const char* new_path) {
    if (!g_root_mounted) return -20;
    if (!old_path || !new_path) return -21;

    char old_parent[128], old_leaf[64];
    char new_parent[128], new_leaf[64];

    int r = vfs_split_parent_leaf(old_path, old_parent, sizeof(old_parent), old_leaf, sizeof(old_leaf));
    if (r) return r;

    r = vfs_split_parent_leaf(new_path, new_parent, sizeof(new_parent), new_leaf, sizeof(new_leaf));
    if (r) return r;

    // same dir only
    int same = 1;
    for (int i = 0;; i++) {
        if (old_parent[i] != new_parent[i]) { same = 0; break; }
        if (old_parent[i] == 0) break;
    }
    if (!same) return -1008;

    uint16_t parent_cluster;
    r = vfs_resolve_dir_cluster(old_parent, &parent_cluster);
    if (r) return r;

    return fat_rename(&g_root, parent_cluster, old_leaf, new_leaf);
}

int vfs_move(const char* old_path, const char* new_path) {
      if (!g_root_mounted) return -20;
    if (!old_path || !new_path) return -21;

    char old_parent[128], old_leaf[64];
    char new_parent[128], new_leaf[64];

    int r = vfs_split_parent_leaf(old_path, old_parent, sizeof(old_parent), old_leaf, sizeof(old_leaf));
    if (r) return r;
    
    r = vfs_split_parent_leaf(new_path, new_parent, sizeof(new_parent), new_leaf, sizeof(new_leaf));
    if (r) return r;

    uint16_t old_parent_cluster;
    r = vfs_resolve_dir_cluster(old_parent, &old_parent_cluster);
    if (r) return r;

    uint16_t new_parent_cluster;
    r = vfs_resolve_dir_cluster(new_parent, &new_parent_cluster);
    if (r) return r;

    return fat_move(&g_root, old_parent_cluster, old_leaf, new_parent_cluster, new_leaf);

    
}

int vfs_write_file(const char* path, const void* data, uint32_t size) {
    if (!g_root_mounted) return -20;
    if (!path) return -21;
    if (size > 0 && !data) return -22;

    char parent[128];
    char leaf[64];

    int r = vfs_split_parent_leaf(path, parent, sizeof(parent), leaf, sizeof(leaf));
    if (r) return r;

    uint16_t parent_cluster;
    r = vfs_resolve_dir_cluster(parent, &parent_cluster);
    if (r) return r;

    return fat_write_file(&g_root, parent_cluster, leaf, data, size);
}

int vfs_append_file(const char* path, const void* data, uint32_t size) {
    if (!g_root_mounted) return -20;
    if (!path) return -21;
    if (size > 0 && !data) return -22;

    char parent[128];
    char leaf[64];

    int r = vfs_split_parent_leaf(path, parent, sizeof(parent), leaf, sizeof(leaf));
    if (r) return r;

    uint16_t parent_cluster;
    r = vfs_resolve_dir_cluster(parent, &parent_cluster);
    if (r) return r;

    return fat_append_file(&g_root, parent_cluster, leaf, data, size);
}


void test() {

 fat_test_dir_write(&g_root);

}

