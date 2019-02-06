#ifndef MINIX_SERVERS_FAT32_FAT32_H_
#define MINIX_SERVERS_FAT32_FAT32_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
}
#endif

constexpr size_t FAT32_MAX_NAME_LEN = 256;
constexpr size_t FAT32_MAX_HANDLES = 4096;
constexpr size_t FAT32_MIN_CLUSTERS	= 65525;
constexpr size_t FAT32_MAX_CLUSTERS	= 268435445;


using datetime_t = struct tm;

/* Attributes that FAT32 files can have. */
enum class FAT32Attributes
{
	FAT32_ATTR_READONLY = 0x01,
	FAT32_ATTR_HIDDEN = 0x02,
	FAT32_ATTR_SYSTEM = 0x04,
	FAT32_ATTR_VOLUMEID = 0x08,
	FAT32_ATTR_DIR = 0x10,
	FAT32_ATTR_ARCHIVE = 0x20
};

template <typename TFlags = uint8_t>
static inline decltype(auto) has_attribute(TFlags flags, FAT32Attributes att)
{
	return flags & static_cast<decltype(flags)>(att);
}

/* Familiarity with FAT32 is needed to understand the following data structures.
 * They are mostly defined by their on-disk format. */

typedef struct fat32_bpb_t
{
	uint8_t header[3];
	uint8_t oem_id[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t tables;
	uint16_t root_direntries;
	uint16_t total_sectors_16;
	uint8_t media_descriptor_type;
	uint16_t sectors_per_table_16;
	uint16_t sectors_per_track;
	uint16_t heads_or_sides;
	uint32_t hidden_sectors;
	uint32_t total_sectors_32;
} __attribute__((packed)) fat32_bpb_t;

typedef struct fat32_ebr_t
{
	uint32_t sectors_per_table_32;
	uint16_t flags;
	uint16_t version;
	uint32_t root_cluster_nr;
	uint16_t fsinfo_cluster_nr;
	uint16_t backup_boot_cluster_nr;
	uint8_t unused_1[12];
	uint8_t drive_nr;
	uint8_t unused_2;
	uint8_t unused_3;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t fat_type_label[8];
} __attribute__((packed)) fat32_ebr_t;

typedef struct fat32_header_t
{
	fat32_bpb_t bpb;
	fat32_ebr_t ebr;
} __attribute__((packed)) fat32_header_t;

typedef struct fat32_info_t
{
	int fat_size_sectors;
	int root_dir_sectors;
	int first_data_sector;
	int first_fat_sector;
	int bytes_per_cluster;
} fat32_info_t;

typedef struct fat32_time_t
{
	uint8_t seconds : 5;
	uint8_t minutes : 6;
	uint8_t hours : 5;
} __attribute__((packed)) fat32_time_t;

typedef struct fat32_date_t
{
	uint8_t day : 5;
	uint8_t month : 4;
	uint8_t year : 7;
} __attribute__((packed)) fat32_date_t;

typedef struct fat32_direntry_t
{
	uint8_t filename_83[11];
	uint8_t attributes;
	uint8_t reserved_1[2];
	fat32_time_t creation_time;
	fat32_date_t creation_date;
	fat32_date_t last_access_date;
	uint16_t first_cluster_nr_high;
	fat32_time_t last_modified_time;
	fat32_date_t last_modified_date;
	uint16_t first_cluster_nr_low;
	uint32_t size_bytes;
} __attribute__((packed)) fat32_direntry_t;

static inline auto direntry_full_addr(fat32_direntry_t *entry)->decltype((entry->first_cluster_nr_high << 16) | entry->first_cluster_nr_low)
{
	return (entry->first_cluster_nr_high << 16) | entry->first_cluster_nr_low;
}

typedef struct fat32_lfn_direntry_t
{
	uint8_t ord;
	uint16_t chars_1[5];
	uint8_t attributes;
	uint8_t lfn_type;
	uint8_t checksum;
	uint16_t chars_2[6];
	uint16_t unused_1;
	uint16_t chars_3[2];
} __attribute__((packed)) fat32_lfn_direntry_t;

typedef struct fat32_fs_t
{
	int nr;
	int is_open;
	int fd;
	endpoint_t opened_by;
	fat32_header_t header;
	fat32_info_t info;
} fat32_fs_t;

typedef struct fat32_dir_t
{
	int nr;
	fat32_fs_t *fs;
	int active_cluster;
	int cluster_buffer_offset;
	int last_entry_start_cluster;
	int last_entry_was_dir;
	int last_entry_size_bytes;
	char *cluster_buffer;
} fat32_dir_t;

typedef struct fat32_file_t
{
	int nr;
	fat32_fs_t *fs;
	int active_cluster;
	int remaining_size;
} fat32_file_t;

typedef struct fat32_entry_t
{
	char filename[FAT32_MAX_NAME_LEN];
	int is_directory;
	int is_readonly;
	int is_hidden;
	int is_system;

	// Only tm_mon, tm_mday, tm_year, tm_sec, tm_hour and tm_sec
	// are set.
	datetime_t creation;

	// Only tm_mon, tm_mday and tm_year set.
	datetime_t access;

	// Only tm_mon, tm_mday, tm_year, tm_sec, tm_hour and tm_sec
	// are set.
	datetime_t modification;

	int size_bytes;
} fat32_entry_t;

// A type that can represent any kind of direntry, be it a short or long one.
typedef union fat32_any_direntry_t {
	fat32_direntry_t short_entry;
	fat32_lfn_direntry_t long_entry;
} fat32_any_direntry_t;

/**
 * @brief Calculates various FAT32 constants and checks if the filesystem is valid
 * FAT32.Refer to the FAT32 documentation for details on the implementation of some of these functions 
 * and some magic numbers used.
 * 
 * @param header The header of current FAT
 * @param dst_info The info to build
 * @return int Status
 */
int build_fat_info(fat32_header_t *header, fat32_info_t *dst_info);

/**
 * @brief Reads the FAT header from an fd.
 * 
 * @param fd 
 * @param header 
 * @return int 
 */
int read_fat_header(int fd, fat32_header_t *dst);

/**
 * @brief  Seeks to a cluster identified by a given cluster numbe.
 * 
 * @param header 
 * @param info 
 * @param fd 
 * @param cluster_nr 
 * @return int 
 */
int seek_cluster(fat32_header_t *header, fat32_info_t *info, int fd, int cluster_nr);

/**
 * @brief Seeks to a given cluster and reads its contents into memory. The buffer given
 * must be at least info->bytes_per_sector bytes long.
 * 
 * @param header the header of current FAT
 * @param info the info of current FAT;
 * @param fd deivce's fd
 * @param cluster_nr cluster ID
 * @param buf the buffer to read
 * @return int Status
 */
int seek_read_cluster(fat32_header_t *header, fat32_info_t *info, int fd, int cluster_nr, char *buf);

/**
 * @brief Looks up the given cluster in the FAT and gets its successor in the cluster
 * chain. Writes -1 to *next_cluster_nr if this is the last cluster in the chain.
 * 
 * @param header 
 * @param info 
 * @param fd 
 * @param cluster_nr 
 * @param next_cluster_nr 
 * @return int 
 */
int get_next_cluster(fat32_header_t *header, fat32_info_t *info, int fd, int cluster_nr, int *next_cluster_nr);

/**
 * @brief Converts a raw FAT32 entry type to a user-friendlier type. The caller must set
 * the filename.
 * 
 * @param entry 
 * @param dest 
 */
void convert_entry(fat32_direntry_t *entry, fat32_entry_t *dest);

#endif