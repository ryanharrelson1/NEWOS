#include "fat.h"
#include "../serial.h"
#include "../../libs/memhelp.h"


typedef struct  __attribute__((packed)) {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} fat_bpb_t;




typedef struct {
    uint32_t lba;
    uint8_t  valid;
    uint8_t  dirty;
    uint8_t  data[512];
} fat_cache_t;

static fat_cache_t g_fat_cache = {0};

#define ATTR_LFN 0x0F
#define ATTR_DIR 0x10

// Helper functions

static uint32_t div_up_32(uint32_t a, uint32_t b) { return (a + b - 1) / b; }

static int disk_read(ide_device_t* dev, uint32_t lba, uint32_t count, uint8_t* buf) {
    uint8_t* p = buf;

    while (count) {
        uint32_t chunk = (count > 256) ? 256 : count;
        uint8_t  c8    = (chunk == 256) ? 0 : (uint8_t)chunk; // 0 => 256
        int r = ide_read_sectors(dev, lba, c8, p);
        if (r) return r;
        lba += chunk;
        p   += chunk * 512;
        count -= chunk;
    }
    return 0;
}

static int disk_write(ide_device_t* dev, uint32_t lba, uint32_t count, const uint8_t* buf) {
    const uint8_t* p = buf;

    while (count) {
        uint32_t chunk = (count > 256) ? 256 : count;
        uint8_t  c8    = (chunk == 256) ? 0 : (uint8_t)chunk; // 0 => 256
        int r = ide_write_sectors(dev, lba, c8, p);
        if (r) return r;
        lba += chunk;
        p   += chunk * 512;
        count -= chunk;
    }
    return 0;
}


static void print_83(const char n[11]) {
    for (int i = 0; i < 8; i++) {
        if (n[i] == ' ') break;
        write_serial(n[i]);
    }
    if (n[8] != ' ') {
        write_serial('.');
        for (int i = 8; i < 11; i++) {
            if (n[i] == ' ') break;
            write_serial(n[i]);
        }
    }
}

static void name_to_83_upper(const char* in, char out11[11]) {
    for (int i = 0; i < 11; i++) out11[i] = ' ';

    int o = 0;
    while (*in && *in != '.' && o < 8) {
        char c = *in++;
        if (c >= 'a' && c <= 'z') c -= 32;
        out11[o++] = c;
    }

    if (*in == '.') in++;
    o = 8;
    while (*in && o < 11) {
        char c = *in++;
        if (c >= 'a' && c <= 'z') c -= 32;
        out11[o++] = c;
    }
}
static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int fat_is_eoc(uint16_t cluster) {
    return cluster >= FAT16_EOC_MIN;
}

static const char* skip_slashes(const char* p) {
    while (*p == '/') p++;
    return p;
}

static const char* next_component(const char* p, char* out, int out_max) {
    int i =0;
    while(*p && *p !='/') {
        if(i< out_max - 1) out[i++] = *p;
        p++;
    }
    out[i] = 0;
    return p;
}

static void fat_file_reset(fat_file_t* file) {
  file->current_cluster = file->start_clusrter;
  file->pos = 0;
  file->current_cluster_index = 0;
  file->current_cluster_sector = 0;
  file->current_offset_within_sector = 0;
}

static int fat_advance_clusters(fat_info_t* fs, uint16_t* cl_io, uint32_t n) {
    uint16_t cl = *cl_io;
    for (uint32_t i = 0; i < n; i++) {
        if (cl < 2 || fat_is_eoc(cl)) return -610; // unexpected end
        cl = fat16_get_fat_entry(fs, cl);
    }
    *cl_io = cl;
    return 0;
}

static int fat_file_reposition(fat_file_t* f, uint32_t new_pos) {
    if (new_pos > f->size) new_pos = f->size;

    // Compute where new_pos lands
    uint32_t bps = f->fs->bytes_per_sector;           // assume 512 in your system
    uint32_t spc = f->fs->sectors_per_cluster;
    uint32_t bytes_per_cluster = bps * spc;

    uint32_t target_cluster_index = (bytes_per_cluster == 0) ? 0 : (new_pos / bytes_per_cluster);
    uint32_t inside_cluster = (bytes_per_cluster == 0) ? 0 : (new_pos % bytes_per_cluster);

    uint32_t target_sector_in_cluster = (bps == 0) ? 0 : (inside_cluster / bps);
    uint32_t target_offset_in_sector  = (bps == 0) ? 0 : (inside_cluster % bps);

    // Strategy:
    // - if seeking forward within current stream, walk forward only
    // - if seeking backwards, reset to start and walk from start
    if (target_cluster_index < f->current_cluster_index) {
        fat_file_reset(f);
    }

    // Walk clusters to target
    uint32_t need = target_cluster_index - f->current_cluster_index;
    if (need) {
        int r = fat_advance_clusters(f->fs, &f->current_cluster, need);
        if (r) return r;
        f->current_cluster_index = target_cluster_index;
    }

    f->current_cluster_sector = target_sector_in_cluster;
    f->current_offset_within_sector  = target_offset_in_sector;
    f->pos = new_pos;
    return 0;
}

static int fat_cache_load(fat_info_t* fs, uint32_t fat_lba) {
    if(g_fat_cache.valid && g_fat_cache.lba == fat_lba) {
        return 0; // cache hit
    }

    if(g_fat_cache.valid && g_fat_cache.dirty) {
        int r = disk_write(fs->device, g_fat_cache.lba, 1, g_fat_cache.data);
        if(r) return r;
        g_fat_cache.dirty = 0;
    }

    int r = disk_read(fs->device, fat_lba, 1, g_fat_cache.data);
    if(r) return r;

    g_fat_cache.lba = fat_lba;
    g_fat_cache.valid = 1;
    g_fat_cache.dirty = 0;
    return 0;
}

static int fat_cache_flush(fat_info_t* fs) {
       if (!g_fat_cache.valid || !g_fat_cache.dirty) return 0;

    int r = disk_write(fs->device, g_fat_cache.lba, 1, g_fat_cache.data);
    if (r) return r;
    g_fat_cache.dirty = 0;
    return 0;

}

uint16_t fat_alloc_cluster(fat_info_t* fs) {
    if (!fs) return 0;

    uint32_t data_sectors = fs->total_sectors - fs->data_start_lba;
    uint32_t total_clusters = data_sectors / fs->sectors_per_cluster;

    for(uint32_t cl = 2; cl < total_clusters + 2; cl++) {
        uint16_t v = fat16_get_fat_entry(fs, (uint16_t)cl);
        if(v == FAT16_FREE) {
            int r = fat16_set_fat_entry(fs, (uint16_t)cl, FAT16_EOC);
            if(r) return 0;
            return (uint16_t)cl;
        }
       
    }
    return 0; // no free cluster
}

int fat16_free_chain(fat_info_t* fs, uint16_t first_cluster) {
  if(!fs) return -720;
    uint16_t cl = first_cluster;

    for(int steps = 0; steps < 65536 && cl >=2 && !fat_is_eoc(cl); steps++) {
        uint16_t next = fat16_get_fat_entry(fs, cl);
        int r = fat16_set_fat_entry(fs, cl, FAT16_FREE);
        if(r) return r;
        cl = next;
    }

    if(cl >=2) {
        int r = fat16_set_fat_entry(fs, cl, FAT16_FREE);
        if(r) return r;
    }

    return 0;
}

static int fat16_alloc_chain(fat_info_t* fs, uint32_t count, uint16_t* out_first) {
    if (!fs || !out_first) return -870;
    *out_first = 0;

    if (count == 0) return 0;

    uint16_t first = 0;
    uint16_t prev = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint16_t cl = fat_alloc_cluster(fs);
        if (cl == 0) {
            // rollback already allocated part
            if (first >= 2) fat16_free_chain(fs, first);
            return -871;
        }

        if (first == 0) {
            first = cl;
        } else {
            fat16_set_fat_entry(fs, prev, cl);
        }

        prev = cl;
    }

    fat16_set_fat_entry(fs, prev, FAT16_EOC);
    *out_first = first;
    return 0;
}

static uint32_t fat16_clusters_needed(fat_info_t* fs, uint32_t size) {
   if(size == 0) return 0; 
    uint32_t bps = fs->bytes_per_sector * fs->sectors_per_cluster;
    return (size + bps - 1) / bps;
}


static int fat16_write_chain_data(fat_info_t* fs, uint16_t first_cluster, const uint8_t* data, uint32_t size) {
    if (!fs) return -872;
    if (size == 0) return 0;
    if (first_cluster < 2) return -873;

    uint16_t cl = first_cluster;
    uint32_t rem = size;

    for(int steps = 0; steps < 4096 && cl >= 2 && !fat_is_eoc(cl); steps++) {

        uint32_t lba = fat16_cluster_to_lba(fs, cl);

        for(uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
           uint8_t sec[512];

           memoryset(sec, 0, sizeof(sec));

           uint32_t to_write = (rem > 512) ? 512 : rem;

           if(to_write > 0) {
               memcopy(sec, data, to_write);
               data += to_write;
                rem -= to_write;
               
           }

            int r = disk_write(fs->device, lba + s, 1, sec);
            if(r) return r;

            if(rem == 0) return 0; // done

        }

        cl = fat16_get_fat_entry(fs, cl);
        
        
    }

    return 0; // we don't consider it an error if the chain ended before we wrote all data, as long as we wrote as much as we could
}

static int fat16_get_last_cluster(fat_info_t* fs, uint16_t first_cluster, uint16_t* out_last) {
     if (!fs || !out_last) return -890;
    if (first_cluster < 2) return -891;

    uint16_t cl = first_cluster;
    for (int steps = 0; steps < 65536; steps++) {
        uint16_t next = fat16_get_fat_entry(fs, cl);
        if (fat_is_eoc(next)) {
            *out_last = cl;
            return 0;
        }
        if (next < 2) return -892;
        cl = next;
    }
    return -893;
}

static int fat16_append_cluster(fat_info_t* fs, uint16_t last_cluster, uint16_t* out_new) {
    if (!fs || !out_new) return -894;

    uint16_t newcl = fat_alloc_cluster(fs);
    if (newcl == 0) return -895;

    int r = fat16_set_fat_entry(fs, last_cluster, newcl);
    if (r) {
        fat16_set_fat_entry(fs, newcl, FAT16_FREE);
        return r;
    }

    r = fat16_set_fat_entry(fs, newcl, FAT16_EOC);
    if (r) {
        fat16_set_fat_entry(fs, last_cluster, FAT16_EOC);
        fat16_set_fat_entry(fs, newcl, FAT16_FREE);
        return r;
    }

    *out_new = newcl;
    return 0;
}

static int fat16_zero_cluster(fat_info_t* fs, uint16_t cluster) {
    if (!fs) return -910;
    if (cluster < 2) return -911;

    uint32_t lba = fat16_cluster_to_lba(fs, cluster);
    uint8_t sec[512];
    memoryset(sec, 0, sizeof(sec));

    for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
        int r = disk_write(fs->device, lba + s, 1, sec);
        if (r) return r;
    }

    return 0;
}

static int fat_dir_is_empty(fat_info_t* fs, uint16_t dir_cluster) {
      if (!fs) return -930;
    if (dir_cluster < 2) return -931;

    uint8_t sec[512];
    uint16_t cl = dir_cluster;

    for (int steps = 0; steps < 4096 && cl >= 2 && !fat_is_eoc(cl); steps++) {
        uint32_t lba = fat16_cluster_to_lba(fs, cl);

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            int r = disk_read(fs->device, lba + s, 1, sec);
            if (r) return r;

            for (uint32_t off = 0; off < 512; off += 32) {
                fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
                uint8_t first = (uint8_t)e->name[0];

                if (first == 0x00) return 1;     // end marker => empty enough
                if (first == 0xE5) continue;     // deleted entry, ignore
                if (e->attr == ATTR_LFN) continue;

                // allow "." entry
                if (e->name[0] == '.' && e->name[1] == ' ') continue;

                // allow ".." entry
                if (e->name[0] == '.' && e->name[1] == '.' && e->name[2] == ' ') continue;

                // anything else means not empty
                return 0;
            }
        }

        cl = fat16_get_fat_entry(fs, cl);
    }

    return 1;
}

static int fat_dir_update_dotdot(fat_info_t* fs, uint16_t dir_cluster, uint16_t new_parent_cluster) {
    if (!fs) return -960;
    if (dir_cluster < 2) return -961;

    uint8_t sec[512];
    uint32_t lba = fat16_cluster_to_lba(fs, dir_cluster);

    int r = disk_read(fs->device, lba, 1, sec);
    if (r) return r;

    // ".." should be second entry (offset 32)
    fat_dir_entry_raw_t* dotdot = (fat_dir_entry_raw_t*)(sec + 32);

    // sanity check it's actually ".."
    if (!(dotdot->name[0] == '.' && dotdot->name[1] == '.')) {
        return -962;
    }

    dotdot->first_cluster_low = new_parent_cluster;

    r = disk_write(fs->device, lba, 1, sec);
    if (r) return r;

    return 0;
}




//----------------------------------------------



int fat_mount(fat_info_t* fs, ide_device_t* device) {
    memoryset(fs, 0, sizeof(*fs));
    fs->device = device;

    uint8_t sec[512];
    int r = disk_read(device, 0, 1, sec);
    if (r) return r;

    uint16_t sig = *(uint16_t*)&sec[510];
    if (sig != 0xAA55) return -100;

    fat_bpb_t* bpb = (fat_bpb_t*)sec;

    fs->bytes_per_sector      = bpb->bytes_per_sector;
    fs->sectors_per_cluster   = bpb->sectors_per_cluster;
    fs->reserved_sector_count = bpb->reserved_sectors;
    fs->num_fats              = bpb->num_fats;
    fs->root_entry_count      = bpb->root_entry_count;
    fs->sectors_per_fat       = bpb->sectors_per_fat_16;
    fs->total_sectors         = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;

    if (fs->bytes_per_sector != 512) return -101;
    if (fs->sectors_per_cluster == 0) return -102;
    if (fs->num_fats == 0) return -103;
    if (fs->sectors_per_fat == 0) return -104;

    fs->root_dir_sectors = div_up_32((uint32_t)fs->root_entry_count * 32, fs->bytes_per_sector);

    fs->fat_start_lba      = fs->reserved_sector_count;
    fs->root_dir_start_lba = fs->fat_start_lba + (uint32_t)fs->num_fats * fs->sectors_per_fat;
    fs->data_start_lba     = fs->root_dir_start_lba + fs->root_dir_sectors;

    serial_write_string("fat16 mounted\n");
    serial_write_string("fat_lba=");  serial_write_hex32(fs->fat_start_lba);      serial_write_string("\n");
    serial_write_string("root_lba="); serial_write_hex32(fs->root_dir_start_lba); serial_write_string("\n");
    serial_write_string("data_lba="); serial_write_hex32(fs->data_start_lba);     serial_write_string("\n");
    return 0;
}

int fat_root_list(fat_info_t* fs) {
    uint8_t sec[512];

    for (uint32_t s = 0; s < fs->root_dir_sectors; s++) {
        int r = disk_read(fs->device, fs->root_dir_start_lba + s, 1, sec);
        if (r) return r;

        for (uint32_t off = 0; off < 512; off += 32) {
            fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
            uint8_t first = (uint8_t)e->name[0];

            if (first == 0x00) return 0;      // end marker
            if (first == 0xE5) continue;      // deleted
            if (e->attr == ATTR_LFN) continue;

            serial_write_string((e->attr & ATTR_DIR) ? "[DIR] " : "      ");
            print_83(e->name);
            serial_write_string(" size=");
            serial_write_hex32(e->file_size);
            serial_write_string(" cl=");
            serial_write_hex32(e->first_cluster_low);
            serial_write_string("\n");
        }
    }
    return 0;
}


int fat_root_find(fat_info_t* fs, const char* name83, fat_dir_entry_t* out_entry) {
    char want[11];
    name_to_83_upper(name83, want);

    uint8_t sec[512];

    for (uint32_t s = 0; s < fs->root_dir_sectors; s++) {
        int r = disk_read(fs->device, fs->root_dir_start_lba + s, 1, sec);
        if (r) return r;

        for (uint32_t off = 0; off < 512; off += 32) {
            fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
            uint8_t first = (uint8_t)e->name[0];

            if (first == 0x00) return -200;
            if (first == 0xE5) continue;
            if (e->attr == ATTR_LFN) continue;

            if (memcmps(e->name, want, 11) == 0) {
                memcopy(out_entry->name_83, e->name, 11);
                out_entry->attr = e->attr;
                out_entry->first_cluster = e->first_cluster_low;
                out_entry->file_size = e->file_size;
                return 0;
            }
        }
    }
    return -200;
}

uint32_t fat16_cluster_to_lba(fat_info_t* fs, uint16_t cluster) {
    return fs->data_start_lba + (cluster - 2) * fs->sectors_per_cluster;

}

uint16_t fat16_get_fat_entry(fat_info_t* fs, uint16_t cluster) {
    // FAT16 entry is 2 bytes
    uint32_t fat_offset = (uint32_t)cluster * 2;
    uint32_t fat_sector = fs->fat_start_lba + (fat_offset / fs->bytes_per_sector);
    uint32_t ent_offset = fat_offset % fs->bytes_per_sector;

    uint8_t sec[512];
    if (disk_read(fs->device, fat_sector, 1, sec) != 0) return 0;

    return read_le16(&sec[ent_offset]);
}

int fat16_set_fat_entry(fat_info_t* fs, uint16_t cluster, uint16_t value) {
    if (!fs) return -700;
    if (cluster < 2) return -701;
    if (fs->bytes_per_sector != 512) return -702;

    uint32_t offset = (uint32_t)cluster * 2;        // FAT16
    uint32_t sector_index = offset / 512;
    uint32_t within = offset % 512;

    for (uint32_t fati = 0; fati < fs->num_fats; fati++) {
        uint32_t lba = fs->fat_start_lba + fati * fs->sectors_per_fat + sector_index;

        uint8_t sec[512];
        int r = disk_read(fs->device, lba, 1, sec);
        if (r) return r;

        sec[within + 0] = (uint8_t)(value & 0xFF);
        sec[within + 1] = (uint8_t)((value >> 8) & 0xFF);

        r = disk_write(fs->device, lba, 1, sec);
        if (r) return r;
    }

    return 0;
}
    



int fat_dir_list(fat_info_t* fs, uint16_t cluster) {
    if(cluster == 0) return fat_root_list(fs);

    uint8_t sec[512];
    uint16_t cl = cluster;

    for(int steps = 0; steps < 4096 && cl >= 2 && !fat_is_eoc(cl); steps++) {
        uint32_t lba = fat16_cluster_to_lba(fs, cl);

        for (uint32_t s =0; s < fs->sectors_per_cluster; s++) {
            int r = disk_read(fs->device, lba + s, 1, sec);
            if (r) return r;

            for(uint32_t off = 0; off < 512; off += 32) {
                fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
                uint8_t first = (uint8_t)e->name[0];

                if(first == 0x00) return 0;
                if(first == 0xE5) continue;
                if(e->attr == ATTR_LFN) continue;
                if (e->name[0] == '.') continue;


                serial_write_string((e->attr & ATTR_DIR) ? "[DIR] " : "      ");
                print_83(e->name);
                serial_write_string(" size=");
                serial_write_hex32(e->file_size);
                serial_write_string(" cl=");
                serial_write_hex32(e->first_cluster_low);
                serial_write_string("\n");
            }
        }
        cl = fat16_get_fat_entry(fs, cl);
    }

    return 0;
}

int fat_dir_find(fat_info_t* fs, uint16_t dir_cluster, const char* name83, fat_dir_entry_t* out) {
    if (dir_cluster == 0) {
        return fat_root_find(fs, name83, out);
    }

    char want[11];
    name_to_83_upper(name83, want);

    uint8_t sec[512];
    uint16_t cl = dir_cluster;

    for (int steps = 0; steps < 4096 && cl >= 2 && !fat_is_eoc(cl); steps++) {
        uint32_t lba = fat16_cluster_to_lba(fs, cl);

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            int r = disk_read(fs->device, lba + s, 1, sec);
            if (r) return r;

            for (uint32_t off = 0; off < 512; off += 32) {
                fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
                uint8_t first = (uint8_t)e->name[0];

                if (first == 0x00) return -200;   // end marker
                if (first == 0xE5) continue;
                if (e->attr == ATTR_LFN) continue;

                if (memcmps(e->name, want, 11) == 0) {
                    memcopy(out->name_83, e->name, 11);
                    out->attr = e->attr;
                    out->first_cluster = e->first_cluster_low;
                    out->file_size = e->file_size;
                    return 0;
                }
            }
        }

        cl = fat16_get_fat_entry(fs, cl);
    }

    return -200;
}

int fat_resolve_path(fat_info_t* fs, const char* path, fat_dir_entry_t* out) {
   if (!path || !out) return -900;

   const char* p = skip_slashes(path);
    if (*p == 0) {
        out->attr = ATTR_DIR;
        out->first_cluster = 0;
        out->file_size = 0;
        return 0;
    }

    uint16_t dir_cl = 0; // start from root
    fat_dir_entry_t ent;


    while(*p) {
        char comp[64];
        p = next_component(p, comp, sizeof(comp));
        p = skip_slashes(p);

        if (comp[0] == '.' && comp[1] == 0) {
            continue;
        }

        if (comp[0] == '.' && comp[1] == '.' && comp[2] == 0) {
            // already at root? stay there
            if (dir_cl == 0) continue;

            fat_dir_entry_t parent;
            int rr = fat_dir_find(fs, dir_cl, "..", &parent);
            if (rr != 0) return rr;

            // parent.first_cluster will be 0 when parent is root
            dir_cl = parent.first_cluster;
            continue;
        }

        int r = fat_dir_find(fs, dir_cl, comp, &ent);
        if (r != 0) return r;

        if(*p){
            if(!(ent.attr & ATTR_DIR)) return -901; // not a directory
            dir_cl = ent.first_cluster;
        }
    }

    *out = ent;
    return 0;

    
}



int fat_open_path(fat_info_t* fs, const char* path, fat_file_t* out_file) {
 if(!fs || !path || !out_file) return -900;

   fat_dir_entry_t ent;
   int r = fat_resolve_path(fs, path, &ent);
    if (r != 0) return r;


    if(ent.attr & ATTR_DIR) return -601; // is a directory
    if(ent.first_cluster < 2 && ent.file_size != 0) return -602; // invalid cluster

    out_file->fs = fs;
    out_file->start_clusrter = ent.first_cluster;
    out_file->size = ent.file_size;
    fat_file_reset(out_file);
    return 0;
}

int fat_seek(fat_file_t* file, uint32_t new_pos) {
    if (!file) return -900;
    return fat_file_reposition(file, new_pos);
}

int fat_read_file(fat_file_t* file, void* buf, uint32_t bytes_to_read, uint32_t* out_bytes_read) {
    if (!file || !buf || !out_bytes_read) return -604;
    *out_bytes_read = 0;

    if(file->pos >= file->size) return 0; // EOF
    uint32_t remaining = file->size - file->pos;
    if (bytes_to_read > remaining) bytes_to_read = remaining;
    if(bytes_to_read == 0) return 0;

    uint8_t sec[512];
    uint8_t* outp = (uint8_t*)buf;

    fat_info_t* fs = file->fs;
    uint32_t bps = fs->bytes_per_sector;
    uint32_t spc = fs->sectors_per_cluster;

    while(bytes_to_read > 0) {
        if(file->current_cluster < 2 || fat_is_eoc(file->current_cluster)) return -605; // unexpected end
       

        uint32_t base_lba = fat16_cluster_to_lba(fs, file->current_cluster);
        uint32_t target_lba = base_lba + file->current_cluster_sector;

        int r = disk_read(fs->device, target_lba, 1, sec);
        if (r) return r;

        uint32_t avail_in_sector = bps - file->current_offset_within_sector;
        uint32_t take = (bytes_to_read < avail_in_sector) ? bytes_to_read : avail_in_sector;

        memcopy(outp, sec + file->current_offset_within_sector, take);

        outp += take;
        bytes_to_read -= take;
        *out_bytes_read += take;

        file->pos += take;
        file->current_offset_within_sector += take;


        if(file->current_offset_within_sector >=bps) {
            file->current_offset_within_sector = 0;
            file->current_cluster_sector++;

            if(file->current_cluster_sector >= spc) {
                file->current_cluster_sector = 0;
                file->current_cluster = fat16_get_fat_entry(fs, file->current_cluster);
                file->current_cluster_index++;
             
            }
        }

        if(file->pos >= file->size) break; // EOF
    }

    return 0;
 

}

int fat_cat_by_cluster(fat_info_t* fs, uint16_t first_cluster, uint32_t file_size) {
    if (!fs) return -890;

    uint32_t remaining = file_size;
    uint16_t cl = first_cluster;

    if (remaining == 0) {
        write_serial('\n');
        return 0;
    }

    for (int steps = 0; steps < 4096 && cl >= 2 && !fat_is_eoc(cl); steps++) {
        uint32_t lba = fat16_cluster_to_lba(fs, cl);

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            uint8_t sec[512];
            int r = disk_read(fs->device, lba + s, 1, sec);
            if (r) return r;

            uint32_t to_print = (remaining > 512) ? 512 : remaining;
            for (uint32_t i = 0; i < to_print; i++) {
                write_serial(sec[i]);
            }

            if (remaining <= 512) {
                write_serial('\n');
                return 0;
            }
            remaining -= 512;
        }

        cl = fat16_get_fat_entry(fs, cl);
    }

    write_serial('\n');
    return 0;
}




int fat_dir_write_raw_at(fat_info_t* fs, const fat_dirent_loc_t* loc, const fat_dir_entry_raw_t* raw) {
    if(!fs || !loc || !raw) return -800;


    uint8_t sec[512];
    int r = disk_read(fs->device, loc->lba, 1, (uint8_t*)sec);
    if(r) return r;

    if(loc->off > 512 - 32) return -801; // invalid offset

    memcopy((uint8_t*)sec + loc->off, raw, 32);

    r = disk_write(fs->device, loc->lba, 1, (uint8_t*)sec);
    return r;

}

int fat_dir_find_loc(fat_info_t* fs, uint16_t dir_cluster, const char* name83, fat_dirent_loc_t* out_loc, fat_dir_entry_t* out_entry)
{
    if (!fs || !name83 || !out_loc || !out_entry) return -802;

    char want[11];
    name_to_83_upper(name83, want);

    uint8_t sec[512];

    // ROOT DIR (fixed area)
    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < fs->root_dir_sectors; s++) {
            uint32_t lba = fs->root_dir_start_lba + s;
            int r = disk_read(fs->device, lba, 1, sec);
            if (r) return r;

            for (uint32_t off = 0; off < 512; off += 32) {
                fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
                uint8_t first = (uint8_t)e->name[0];

                if (first == 0x00) return -200; // end marker => not found
                if (first == 0xE5) continue;
                if (e->attr == ATTR_LFN) continue;

                if (memcmps(e->name, want, 11) == 0) {
                    out_loc->dir_cluster = 0;
                    out_loc->lba = lba;
                    out_loc->off = (uint16_t)off;
                    memcopy(out_entry, e, 32);
                    return 0;
                }
            }
        }
        return -200;
    }

    // SUBDIR (cluster chain)
    uint16_t cl = dir_cluster;
    for (int steps = 0; steps < 4096 && cl >= 2 && !fat_is_eoc(cl); steps++) {
        uint32_t base = fat16_cluster_to_lba(fs, cl);

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            uint32_t lba = base + s;
            int r = disk_read(fs->device, lba, 1, sec);
            if (r) return r;

            for (uint32_t off = 0; off < 512; off += 32) {
                fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
                uint8_t first = (uint8_t)e->name[0];

                if (first == 0x00) return -200;
                if (first == 0xE5) continue;
                if (e->attr == ATTR_LFN) continue;

                if (memcmps(e->name, want, 11) == 0) {
                    out_loc->dir_cluster = dir_cluster;
                    out_loc->lba = lba;
                    out_loc->off = (uint16_t)off;
                    memcopy(out_entry, e, 32);
                    return 0;
                }
            }
        }

        cl = fat16_get_fat_entry(fs, cl);
    }

    return -200;
}

int fat_dir_grow(fat_info_t* fs, uint16_t dir_cluster, uint16_t* out_new_cluster) {
    if (!fs || !out_new_cluster) return -980;
    if (dir_cluster < 2) return -981; // root cannot grow in FAT16

    uint16_t last;
    int r = fat16_get_last_cluster(fs, dir_cluster, &last);
    if (r) return r;

    uint16_t newcl = fat_alloc_cluster(fs);
    if (newcl == 0) return -982;

    // last -> newcl
    r = fat16_set_fat_entry(fs, last, newcl);
    if (r) {
        fat16_set_fat_entry(fs, newcl, FAT16_FREE);
        return r;
    }

    // newcl -> EOC
    r = fat16_set_fat_entry(fs, newcl, FAT16_EOC);
    if (r) {
        fat16_set_fat_entry(fs, last, FAT16_EOC);
        fat16_set_fat_entry(fs, newcl, FAT16_FREE);
        return r;
    }

    r = fat16_zero_cluster(fs, newcl);
    if (r) {
        fat16_set_fat_entry(fs, last, FAT16_EOC);
        fat16_set_fat_entry(fs, newcl, FAT16_FREE);
        return r;
    }

    *out_new_cluster = newcl;
    return 0;
}

int fat_dir_find_free_slot(fat_info_t* fs, uint16_t dir_cluster, fat_dirent_loc_t* out_loc) {
    if (!fs || !out_loc) return -803;

    uint8_t sec[512];

    // ROOT DIR: fixed-size in FAT16, cannot grow
    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < fs->root_dir_sectors; s++) {
            uint32_t lba = fs->root_dir_start_lba + s;
            int r = disk_read(fs->device, lba, 1, sec);
            if (r) return r;

            for (uint32_t off = 0; off < 512; off += 32) {
                fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
                uint8_t first = (uint8_t)e->name[0];

                if (first == 0x00 || first == 0xE5) {
                    out_loc->dir_cluster = 0;
                    out_loc->lba = lba;
                    out_loc->off = (uint16_t)off;
                    return 0;
                }
            }
        }
        return -804; // root full
    }

    // SUBDIR: search existing chain first
    uint16_t cl = dir_cluster;
    uint16_t last_cl = dir_cluster;

    for (int steps = 0; steps < 4096 && cl >= 2 && !fat_is_eoc(cl); steps++) {
        last_cl = cl;
        uint32_t base = fat16_cluster_to_lba(fs, cl);

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            uint32_t lba = base + s;
            int r = disk_read(fs->device, lba, 1, sec);
            if (r) return r;

            for (uint32_t off = 0; off < 512; off += 32) {
                fat_dir_entry_raw_t* e = (fat_dir_entry_raw_t*)(sec + off);
                uint8_t first = (uint8_t)e->name[0];

                if (first == 0x00 || first == 0xE5) {
                    out_loc->dir_cluster = dir_cluster;
                    out_loc->lba = lba;
                    out_loc->off = (uint16_t)off;
                    return 0;
                }
            }
        }

        uint16_t next = fat16_get_fat_entry(fs, cl);
        if (fat_is_eoc(next)) break;
        cl = next;
    }

    // No free slot found -> grow the subdir
    uint16_t newcl;
    int r = fat_dir_grow(fs, dir_cluster, &newcl);
    if (r) return r;

    // First slot of first sector in new cluster is free
    out_loc->dir_cluster = dir_cluster;
    out_loc->lba = fat16_cluster_to_lba(fs, newcl);
    out_loc->off = 0;
    return 0;
}

int fat_unlink(fat_info_t* fs, uint16_t dir_cluster, const char* name83){
    if(!fs || !name83) return -850;

    fat_dirent_loc_t loc;
    fat_dir_entry_raw_t raw;

    int r = fat_dir_find_loc(fs, dir_cluster, name83, &loc, &raw);
    if(r) return r;

    if(raw.attr & ATTR_DIR) return -851; // is a directory

    uint16_t first_cluster = raw.first_cluster_low;

    // mark directory entry as deleted
    raw.name[0] = 0xE5;
    r = fat_dir_write_raw_at(fs, &loc, &raw);
    if(r) return r;

    if(first_cluster >= 2) {
        r = fat16_free_chain(fs, first_cluster);
        if(r) return r;
    }

    return 0;
    
}

int fat_create_file(fat_info_t* fs, uint16_t dir_cluster, const char* name83) {
    if(!fs || !name83) return -850;

   fat_dir_entry_t existing;
    int r = fat_dir_find(fs, dir_cluster, name83, &existing);
    if(r == 0) return -852; // already exists

    fat_dirent_loc_t loc;
    r = fat_dir_find_free_slot(fs, dir_cluster, &loc);
    if(r) return r;

    fat_dir_entry_raw_t new_entry;
    memoryset(&new_entry, 0, sizeof(new_entry));

    name_to_83_upper(name83, new_entry.name);

    new_entry.attr = 0x20; // regular file
    new_entry.first_cluster_low = 0; // no clusters allocated yet
    new_entry.file_size = 0;

    r = fat_dir_write_raw_at(fs, &loc, &new_entry);
    if(r) return r;

    return 0;
}

int fat_write_file(fat_info_t* fs, uint16_t dir_cluster, const char* name83, const void* data, uint32_t size){
    if(!fs || !name83) return -850;
    if(size > 0 && !data) return -881;
   

    fat_dirent_loc_t loc;
    fat_dir_entry_raw_t raw;

    int r = fat_dir_find_loc(fs, dir_cluster, name83, &loc, &raw);
    if(r) return r;
     

    if(raw.attr & ATTR_DIR) return -851; // is a directory

    if(raw.first_cluster_low >= 2) {
        r = fat16_free_chain(fs, raw.first_cluster_low);
        if(r) return r;
        raw.first_cluster_low = 0;
    }

    uint16_t first = 0;

    uint32_t needed_clusters = fat16_clusters_needed(fs, size);


    if(needed_clusters > 0) {
         
       r = fat16_alloc_chain(fs, needed_clusters, &first);
        if(r) return r;
        
       
       
        r = fat16_write_chain_data(fs, first, (const uint8_t*)data, size); 
        if(r) {
            
            fat16_free_chain(fs, first);
            return r;
        }
         
        raw.first_cluster_low = first;
    } else {
       
        raw.first_cluster_low = 0;
    }
    raw.file_size = size;
    

    r = fat_dir_write_raw_at(fs, &loc, &raw);
    if(r) return r;

    

    return 0;
}

int fat_append_file(
    fat_info_t* fs,
    uint16_t dir_cluster,
    const char* name83,
    const void* data,
    uint32_t size
) {
    if (!fs || !name83) return -900;
    if (size == 0) return 0;
    if (!data) return -901;

    fat_dirent_loc_t loc;
    fat_dir_entry_raw_t raw;

    int r = fat_dir_find_loc(fs, dir_cluster, name83, &loc, &raw);
    if (r) return r;

    if (raw.attr & ATTR_DIR) return -902;

    const uint8_t* in = (const uint8_t*)data;
    uint32_t remaining = size;
    uint32_t old_size = raw.file_size;

    uint32_t bps = fs->bytes_per_sector;
    uint32_t spc = fs->sectors_per_cluster;
    uint32_t bytes_per_cluster = bps * spc;

    uint16_t first_cluster = raw.first_cluster_low;
    uint16_t cur_cluster = first_cluster;

    // empty file: allocate first cluster if needed
    if (first_cluster < 2) {
        first_cluster = fat_alloc_cluster(fs);
        if (first_cluster == 0) return -903;
        raw.first_cluster_low = first_cluster;
        cur_cluster = first_cluster;
    } else {
        r = fat16_get_last_cluster(fs, first_cluster, &cur_cluster);
        if (r) return r;
    }

    uint32_t off_in_file = old_size;
    uint32_t off_in_cluster = off_in_file % bytes_per_cluster;
    uint32_t sector_in_cluster = off_in_cluster / bps;
    uint32_t off_in_sector = off_in_cluster % bps;

    while (remaining > 0) {
        uint32_t base_lba = fat16_cluster_to_lba(fs, cur_cluster);
        uint32_t lba = base_lba + sector_in_cluster;

        uint8_t sec[512];

        // partial sector: must read-modify-write
        if (off_in_sector != 0 || remaining < 512) {
            r = disk_read(fs->device, lba, 1, sec);
            if (r) return r;
        } else {
            memoryset(sec, 0, sizeof(sec));
        }

        uint32_t avail = bps - off_in_sector;
        uint32_t take = (remaining < avail) ? remaining : avail;

        memcopy(sec + off_in_sector, in, take);

        r = disk_write(fs->device, lba, 1, sec);
        if (r) return r;

        in += take;
        remaining -= take;
        off_in_file += take;
        off_in_sector += take;

        if (off_in_sector >= bps) {
            off_in_sector = 0;
            sector_in_cluster++;

            if (sector_in_cluster >= spc && remaining > 0) {
                sector_in_cluster = 0;

                uint16_t next = fat16_get_fat_entry(fs, cur_cluster);
                if (fat_is_eoc(next)) {
                    r = fat16_append_cluster(fs, cur_cluster, &next);
                    if (r) return r;
                }
                cur_cluster = next;
            }
        }
    }

    raw.file_size = old_size + size;
    r = fat_dir_write_raw_at(fs, &loc, &raw);
    if (r) return r;

    return 0;
}

int fat_mkdir(fat_info_t* fs, uint16_t parent_dir_cluster, const char* name83) {
    if(!fs || !name83) return -920;

    fat_dir_entry_t existing;
     int r = fat_dir_find(fs, parent_dir_cluster, name83, &existing);
    if(r == 0) return -921; // already exists


    fat_dirent_loc_t loc;
    r = fat_dir_find_free_slot(fs, parent_dir_cluster, &loc);
    if(r) return r;

    uint16_t new_dir_cluster = fat_alloc_cluster(fs);
    if(new_dir_cluster == 0) return -922;


    r = fat16_zero_cluster(fs, new_dir_cluster);
    if(r) {
        fat16_set_fat_entry(fs, new_dir_cluster, FAT16_FREE);
        return r;
    }

    fat_dir_entry_raw_t dot;
    memoryset(&dot, 0, sizeof(dot));
    for (int i = 0; i < 11; i++) dot.name[i] = ' ';
    dot.name[0] = '.';
    dot.attr = ATTR_DIR;
    dot.first_cluster_low = new_dir_cluster;
    dot.file_size = 0;


    fat_dir_entry_raw_t dotdot;
    memoryset(&dotdot, 0, sizeof(dotdot));
    for (int i = 0; i < 11; i++) dotdot.name[i] = ' ';
    dotdot.name[0] = '.';
    dotdot.name[1] = '.';
    dotdot.attr = ATTR_DIR;
    dotdot.first_cluster_low = ( parent_dir_cluster == 0) ? 0 : parent_dir_cluster;
    dotdot.file_size = 0;

    uint32_t lba = fat16_cluster_to_lba(fs, new_dir_cluster);
    uint8_t sec[512];
    memoryset(sec, 0, sizeof(sec));
    memcopy(sec + 0, &dot, 32);
    memcopy(sec + 32, &dotdot, 32);

    r = disk_write(fs->device, lba, 1, sec);
    if(r) {
        fat16_free_chain(fs, new_dir_cluster);
        return r;
    }

    fat_dir_entry_raw_t parent_update;
    memoryset(&parent_update, 0, sizeof(parent_update));
    name_to_83_upper(name83, parent_update.name);
    parent_update.attr = ATTR_DIR;
    parent_update.first_cluster_low = new_dir_cluster;
    parent_update.file_size = 0;

    r = fat_dir_write_raw_at(fs, &loc, &parent_update);
    if(r) {
        fat16_free_chain(fs, new_dir_cluster);
        return r;
    }

    

 return 0;

}

int fat_rmdir(fat_info_t* fs, uint16_t parent_dir_cluster, const char* name83) {
   if(!fs || !name83) return -940;

   fat_dirent_loc_t loc;
   fat_dir_entry_raw_t raw;

   int r = fat_dir_find_loc(fs, parent_dir_cluster, name83, &loc, &raw);

    if(r) return r;

    if(!(raw.attr & ATTR_DIR)) return -941; // not a directory

    uint16_t dir_cluster = raw.first_cluster_low;
    if(dir_cluster < 2) return -942; // invalid cluster

    r = fat_dir_is_empty(fs, dir_cluster);
    if(r < 0) return r;
    if(r == 0) return -943; // not empty

    raw.name[0] = 0xE5; // mark as deleted
    r = fat_dir_write_raw_at(fs, &loc, &raw);
    if(r) return r;

    r = fat16_free_chain(fs, dir_cluster);
    if(r) return r;

    return 0;
    

    
}

int fat_rename(fat_info_t* fs, uint16_t dir_cluster, const char* old_name83, const char* new_name83) {
    if (!fs || !old_name83 || !new_name83) return -950;

    // find old entry
    fat_dirent_loc_t loc;
    fat_dir_entry_raw_t raw;

    int r = fat_dir_find_loc(fs, dir_cluster, old_name83, &loc, &raw);
    if (r) return r;

    // reject rename of "." or ".."
    if (raw.name[0] == '.') return -951;

    // ensure new name does not already exist
    fat_dir_entry_t existing;
    r = fat_dir_find(fs, dir_cluster, new_name83, &existing);
    if (r == 0) return -952;     // target already exists
    if (r != -200) return r;     // real error

    // replace name
    name_to_83_upper(new_name83, raw.name);

    // write updated entry back
    r = fat_dir_write_raw_at(fs, &loc, &raw);
    if (r) return r;

    return 0;
}

int fat_move(fat_info_t* fs, uint16_t src_dir_cluster, const char* name83, uint16_t dest_dir_cluster, const char* new_name83) {
   if(!fs || !name83 || !new_name83) return -970;

   fat_dirent_loc_t src_loc;
    fat_dir_entry_raw_t src_raw;

    int r = fat_dir_find_loc(fs, src_dir_cluster, name83, &src_loc, &src_raw);
    if(r) return r;

    if(src_raw.name[0] == '.') return -971; // reject move of "."

    fat_dir_entry_t existing;
    r = fat_dir_find(fs, dest_dir_cluster, new_name83, &existing);
    if(r == 0) return -972; // target already exists
    if(r != -200) return r; // real error

    fat_dirent_loc_t dest_loc;
    r = fat_dir_find_free_slot(fs, dest_dir_cluster, &dest_loc);
    if(r) return r;

    fat_dir_entry_raw_t new_entry = src_raw;
    name_to_83_upper(new_name83, new_entry.name);

    r = fat_dir_write_raw_at(fs, &dest_loc, &new_entry);
    if(r) return r;

    if(new_entry.attr & ATTR_DIR) {
        // if moving a directory, must also update its "." entry's cluster number (if it has one)
      uint16_t moved_dir_cluster = src_raw.first_cluster_low;
      uint16_t parent_ref = (dest_dir_cluster == 0) ? 0 : dest_dir_cluster;

      r = fat_dir_update_dotdot(fs, moved_dir_cluster, parent_ref);
        if(r){
            new_entry.name[0] = (char)0xE5; // mark new entry as deleted
            fat_dir_write_raw_at(fs, &dest_loc, &new_entry);
            return r;
        }
       
    }

    src_raw.name[0] = (char)0xE5; // mark source entry as deleted
    r = fat_dir_write_raw_at(fs, &src_loc, &src_raw);
    if(r) return r;


    return 0;
}


   

void fat_test_dir_write(fat_info_t* fs) {
 fat_dir_entry_t ent;
    int r = fat_dir_find(fs, 3, "NEWDIR", &ent);
    serial_write_string("find NEWDIR r=");
    serial_write_hex32(r);
    serial_write_string(" cl=");
    serial_write_hex32(ent.first_cluster);
    serial_write_string("\n");
    if (r) return;

    for (int i = 0; i < 20; i++) {
        char name[13];

        // produce F00.TXT, F01.TXT, ...
        name[0] = 'F';
        name[1] = '0' + ((i / 10) % 10);
        name[2] = '0' + (i % 10);
        name[3] = '.';
        name[4] = 'T';
        name[5] = 'X';
        name[6] = 'T';
        name[7] = 0;

        r = fat_create_file(fs, ent.first_cluster, name);
        serial_write_string("create ");
        serial_write_string(name);
        serial_write_string(" r=");
        serial_write_hex32(r);
        serial_write_string("\n");
    }

    serial_write_string("after growth test:\n");
    fat_dir_list(fs, ent.first_cluster);


}







