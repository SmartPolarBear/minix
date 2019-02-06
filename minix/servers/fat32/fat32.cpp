#include "inc.h"
#include "proto.h"
#include "fat32.h"
#include <unistd.h>
#include <minix/com.h>
#include <string.h>

static void fat32_time_to_tm(const fat32_time_t &time, datetime_t *t);
static void fat32_date_to_tm(const fat32_date_t &date, datetime_t *t);
static void extract_properties(const fat32_direntry_t *entry, fat32_entry_t *dest);
static auto verify_header(fat32_header_t *header) -> bool;
static auto verify_cluster(size_t total_clusters) -> bool;
template<typename T>
static inline void zero(T *mem);



static void fat32_time_to_tm(const fat32_time_t &time, datetime_t *t)
{
	t->tm_hour = time.hours;
	t->tm_min = time.minutes;
	t->tm_sec = time.seconds * 2;
}

static void fat32_date_to_tm(const fat32_date_t &date, datetime_t *t)
{
	t->tm_year = date.year + 80;
	t->tm_mon = date.month - 1;
	t->tm_mday = date.day;
}

static void extract_properties(const fat32_direntry_t *entry, fat32_entry_t *dest)
{
	dest->is_directory = entry->attributes & static_cast<decltype(entry->attributes)>(FAT32Attributes::FAT32_ATTR_DIR);
	dest->is_readonly = entry->attributes & static_cast<decltype(entry->attributes)>(FAT32Attributes::FAT32_ATTR_READONLY);
	dest->is_hidden = entry->attributes & static_cast<decltype(entry->attributes)>(FAT32Attributes::FAT32_ATTR_HIDDEN);
	dest->is_system = entry->attributes & static_cast<decltype(entry->attributes)>(FAT32Attributes::FAT32_ATTR_SYSTEM);
}

static auto verify_header(fat32_header_t *header) -> bool
{
	return header->bpb.header[0] == 0xEB && header->bpb.header[2] == 0x90;
}

static auto verify_cluster(size_t total_clusters) -> bool
{
	return total_clusters >= FAT32_MIN_CLUSTERS && total_clusters < FAT32_MAX_CLUSTERS;
}

template<typename T>
static inline void zero(T *mem)
{
	memset(mem,0,sizeof(*mem));
}

/**
 * @brief Calculates various FAT32 constants and checks if the filesystem is valid
 * FAT32.Refer to the FAT32 documentation for details on the implementation of some of these functions 
 * and some magic numbers used.
 * 
 * @param header The header of current FAT
 * @param dst_info The info to build
 * @return int Status
 */
int build_fat_info(fat32_header_t *header, fat32_info_t *dst_info)
{
	if (!verify_header(header))
	{
		return FAT32_ERR_NOT_FAT;
	}

	dst_info->bytes_per_cluster =
		header->bpb.bytes_per_sector * header->bpb.sectors_per_cluster;
	dst_info->fat_size_sectors =
		(header->bpb.sectors_per_table_16 == 0) ? header->ebr.sectors_per_table_32
												: header->bpb.sectors_per_table_16;
	dst_info->root_dir_sectors =
		((header->bpb.root_direntries * 32) + (header->bpb.bytes_per_sector - 1)) / header->bpb.bytes_per_sector;
	dst_info->first_data_sector =
		header->bpb.reserved_sectors + (header->bpb.tables * dst_info->fat_size_sectors) +
		dst_info->root_dir_sectors;
	dst_info->first_fat_sector =
		header->bpb.reserved_sectors;
	auto total_sectors =
		header->bpb.total_sectors_16 == 0 ? header->bpb.total_sectors_32
										  : header->bpb.total_sectors_16;
	auto data_sectors =
		total_sectors - (header->bpb.reserved_sectors + (header->bpb.tables * dst_info->fat_size_sectors) +
						 dst_info->root_dir_sectors);
	auto total_clusters = data_sectors / header->bpb.sectors_per_cluster;
	if (!verify_cluster(total_clusters))
	{
		// Is probably FAT, but not FAT32
		return FAT32_ERR_NOT_FAT;
	}

	return OK;
}

/**
 * @brief Reads the FAT header from an fd.
 * 
 * @param fd 
 * @param header 
 * @return int 
 */
int read_fat_header(int fd, fat32_header_t *header)
{
	if (read(fd, header, sizeof(fat32_header_t)) < sizeof(fat32_header_t))
	{
		return FAT32_ERR_IO;
	}

	return OK;
}

/**
 * @brief  Seeks to a cluster identified by a given cluster numbe.
 * 
 * @param header 
 * @param info 
 * @param fd 
 * @param cluster_nr 
 * @return int 
 */
int seek_cluster(fat32_header_t *header, fat32_info_t *info, int fd, int cluster_nr)
{
	auto first_sector_of_cluster =
		((cluster_nr - 2) * header->bpb.sectors_per_cluster) +
		info->first_data_sector;

	auto first_byte_of_sector = first_sector_of_cluster * header->bpb.bytes_per_sector;

	auto ret = lseek(fd, static_cast<off_t>(first_byte_of_sector), SEEK_SET);
	if (ret != static_cast<off_t>(first_byte_of_sector))
	{
		return FAT32_ERR_IO;
	}

	return OK;
}

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
int seek_read_cluster(fat32_header_t *header, fat32_info_t *info, int fd, int cluster_nr, char *buf)
{
	int ret = OK;
	if ((ret = seek_cluster(header, info, fd, cluster_nr)) != OK)
	{
		return ret;
	}

	if (read(fd, buf, info->bytes_per_cluster) != info->bytes_per_cluster)
	{
		return FAT32_ERR_IO;
	}

	return OK;
}

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
int get_next_cluster(fat32_header_t *header, fat32_info_t *info, int fd, int cluster_nr, int *next_cluster_nr)
{
	// This can be cached for better speedz.
	uint32_t next_cluster = 0U;
	auto fat_offset = cluster_nr * 4;
	auto fat_entry_offset =
		(info->first_fat_sector * header->bpb.bytes_per_sector) +
		fat_offset;

	FAT_LOG_PRINTF(debug, "Seeking to offset %d", fat_entry_offset);
	auto ret = lseek(fd, static_cast<off_t>(fat_entry_offset), SEEK_SET);
	if (ret != static_cast<off_t>(fat_entry_offset))
	{
		FAT_LOG_PRINTF(warn, "Seek to FAT at %d failed: %d.", fat_entry_offset, (int)ret);
		return FAT32_ERR_IO;
	}

	auto nread = read(fd, &next_cluster, sizeof(next_cluster));
	if (nread != sizeof(next_cluster))
	{
		FAT_LOG_PRINTF(warn, "Reading FAT entry at %d failed: %d.", fat_entry_offset, (int)nread);
		return FAT32_ERR_IO;
	}

	next_cluster &= 0x0fffffff;
	if (next_cluster >= 0x0ffffff8)
	{
		// Signal that this is the end of the cluster chain.
		FAT_LOG_PRINTF(debug, "No cluster follows cluster %d", cluster_nr);
		*next_cluster_nr = -1;
	}
	else
	{
		FAT_LOG_PRINTF(debug, "Cluster %d follows cluster %d", next_cluster, cluster_nr);
		*next_cluster_nr = next_cluster;
	}

	return OK;
}

/**
 * @brief Converts a raw FAT32 entry type to a user-friendlier type. The caller must set
 * the filename.
 * 
 * @param entry 
 * @param dest 
 */
void convert_entry(fat32_direntry_t *entry, fat32_entry_t *dest)
{
	extract_properties(entry, dest);
	// dest->is_directory = entry->attributes & static_cast<decltype(entry->attributes)>(FAT32Attributes::FAT32_ATTR_DIR);
	// dest->is_readonly = entry->attributes & static_cast<decltype(entry->attributes)>(FAT32Attributes::FAT32_ATTR_READONLY);
	// dest->is_hidden = entry->attributes & static_cast<decltype(entry->attributes)>(FAT32Attributes::FAT32_ATTR_HIDDEN);
	// dest->is_system = entry->attributes & static_cast<decltype(entry->attributes)>(FAT32Attributes::FAT32_ATTR_SYSTEM);

	dest->size_bytes = entry->size_bytes;

	// memset(&dest->modification, 0, sizeof(dest->modification));
	zero(&dest->modification);
	fat32_date_to_tm(entry->last_modified_date, &dest->modification);
	fat32_time_to_tm(entry->last_modified_time, &dest->modification);

	//memset(&dest->access, 0, sizeof(dest->access));
	zero(&dest->access);
	fat32_date_to_tm(entry->last_access_date, &dest->access);

	// memset(&dest->creation, 0, sizeof(dest->creation));
	zero(&dest->creation);
	fat32_date_to_tm(entry->creation_date, &dest->creation);
	fat32_time_to_tm(entry->creation_time, &dest->creation);
}
