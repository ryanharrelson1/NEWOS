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


typedef struct  __attribute__((packed)) {
    char     name[11];
    uint8_t  attr;
    uint8_t  ntres;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat_dir_entry_raw_t;

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







