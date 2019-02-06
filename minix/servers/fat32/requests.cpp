#include <sys/errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "proto.h"
#include <minix/safecopies.h>
#include <minix/syslib.h>
#include <string.h>
#include "fat32.h"
#include <unistd.h>

#define FIND_HANDLE(type, h)                      \
	for (int i = 0; i < type##_handle_count; i++) \
	{                                             \
		if (type##_handles[i].nr == h)            \
		{                                         \
			return &type##_handles[i];            \
		}                                         \
	}                                             \
	return NULL

fat32_fs_t *find_fs_handle(int h)
{
	FIND_HANDLE(fs, h);
}

fat32_dir_t *find_dir_handle(int h)
{
	FIND_HANDLE(dir, h);
}

fat32_file_t *find_file_handle(int h)
{
	FIND_HANDLE(file, h);
}

#define DESTROY_HANDLE(type, ph)                   \
	*ph = type##_handles[type##_handle_count - 1]; \
	type##_handle_count--;

#define CREATE_HANDLE(type, handle)                      \
	do                                                   \
	{                                                    \
		if (type##_handle_count >= FAT32_MAX_HANDLES)    \
		{                                                \
			return -1;                                   \
		}                                                \
		int nr = type##_handle_next++;                   \
		handle = &type##_handles[type##_handle_count++]; \
		handle->nr = nr;                                 \
	} while (0)

int do_open_fs(const char *device, endpoint_t who)
{
	int ret = OK;
	fat32_fs_t *handle;
	CREATE_HANDLE(fs, handle);

	int fd = open(device, O_RDONLY);
	if (fd < 0)
	{
		ret = FAT32_ERR_IO;
		goto destroy_handle;
	}

	if ((ret = read_fat_header(fd, &handle->header)) != OK)
	{
		goto close_fd;
	}

	if ((ret = build_fat_info(&handle->header, &handle->info)) != OK)
	{
		goto close_fd;
	}

	handle->is_open = TRUE;
	handle->fd = fd;
	handle->opened_by = who;

	return handle->nr;

close_fd:
	close(fd);

destroy_handle:
	fs_handle_count--;
	fs_handle_next--;

	return ret;
}

int do_open_root_directory(fat32_fs_t *fs, endpoint_t who)
{
	// 	int ret = OK;
	// 	fat32_dir_t *handle;
	// 	CREATE_HANDLE(dir, handle);

	// 	char* buf = (char*) malloc(fs->info.bytes_per_cluster);
	// 	if (!buf) {
	// 		ret = ENOMEM;
	// 		goto destroy_handle;
	// 	}

	// 	int cluster_nr = fs->header.ebr.root_cluster_nr;
	// 	if ((ret = seek_read_cluster(&fs->header, &fs->info, fs->fd, cluster_nr, buf)) != OK) {
	// 		goto dealloc_buffer;
	// 	}

	// 	handle->fs = fs;
	// 	handle->active_cluster = cluster_nr;
	// 	handle->cluster_buffer_offset = 0;
	// 	handle->cluster_buffer = buf;

	// 	return handle->nr;

	// dealloc_buffer:
	// 	free(buf);

	// destroy_handle:
	// 	dir_handle_count--;
	// 	dir_handle_next--;

	// 	return ret;
	int ret = OK;
	fat32_dir_t *handle;
	CREATE_HANDLE(dir, handle);

	do
	{
		char *buf = (char *)malloc(fs->info.bytes_per_cluster);
		if (!buf)
		{
			ret = ENOMEM;
			// goto destroy_handle;
			break;
		}

		int cluster_nr = fs->header.ebr.root_cluster_nr;
		if ((ret = seek_read_cluster(&fs->header, &fs->info, fs->fd, cluster_nr, buf)) != OK)
		{
			// goto dealloc_buffer;
			free(buf);
			break;
		}

		handle->fs = fs;
		handle->active_cluster = cluster_nr;
		handle->cluster_buffer_offset = 0;
		handle->cluster_buffer = buf;

		return handle->nr;
	} while (0);

	dir_handle_count--;
	dir_handle_next--;

	return ret;
}

int advance_dir_cluster(fat32_dir_t *dir)
{
	int ret, next_cluster_nr;
	if ((ret = get_next_cluster(&dir->fs->header, &dir->fs->info, dir->fs->fd,
								dir->active_cluster, &next_cluster_nr)) != OK)
	{
		return ret;
	}

	if (next_cluster_nr != -1)
	{
		dir->active_cluster = next_cluster_nr;
		dir->cluster_buffer_offset = 0;

		// Advancing the directory cluster also means we have to read in the
		// cluster's contents into the buffer.
		if ((ret = seek_read_cluster(&dir->fs->header, &dir->fs->info,
									 dir->fs->fd, next_cluster_nr, dir->cluster_buffer)) != OK)
		{
			return ret;
		}
	}
	else
	{
		// This signifies 'no more clusters' to do_read_dir_entry.
		dir->cluster_buffer_offset = -1;
	}

	return OK;
}

int advance_file_cluster(fat32_file_t *file)
{
	int ret, next_cluster_nr;
	if ((ret = get_next_cluster(&file->fs->header, &file->fs->info, file->fs->fd,
								file->active_cluster, &next_cluster_nr)) != OK)
	{
		return ret;
	}

	file->active_cluster = next_cluster_nr;
	return OK;
}

void filename_83_to_string(char *filename_83, char *dest)
{
	// Copy the filename portion to the new string.
	strncpy(dest, filename_83, 8);

	// Find the first space in the filename and put a dot after it.
	int first_space;
	for (first_space = 0;
		 first_space < 8 && filename_83[first_space] != ' ';
		 first_space++)
		;

	dest[first_space] = '.';

	// Copy the extension right after the dot.
	strncpy(dest + first_space + 1, filename_83 + 8, 3);

	// Find the first space in the extension and put a null terminator after
	// it.
	int dot_position = first_space;
	for (first_space = 8;
		 first_space < 11 && filename_83[first_space] != ' ';
		 first_space++)
		;

	// If there's no extension, don't put the dot, just overwrite it with
	// a null terminator.
	if (first_space == 8)
	{
		dest[dot_position] = '\0';
	}
	else
	{
		dest[dot_position + first_space - 8 + 1] = '\0';
	}
}

int do_read_dir_entry(fat32_dir_t *dir, fat32_entry_t *dst, int *was_written,
					  endpoint_t who)
{
	*was_written = FALSE;
	dir->last_entry_start_cluster = -1;
	dir->last_entry_was_dir = FALSE;

	if (dir->cluster_buffer_offset == -1)
	{
		// Thhe whole dir was read during a previous call.
		return OK;
	}

	// We'll build the filename from behind, in this buffer, as we encounter the long
	// direntries in opposite order.
	char filename_buf[FAT32_MAX_NAME_LEN];
	memset(filename_buf, 0, FAT32_MAX_NAME_LEN);
	char *pfname = filename_buf + FAT32_MAX_NAME_LEN - 2; // Leave space for a single null terminator

	int seen_long_direntry = FALSE; // Whether we've seen a single long direntry. If we have, then we
									// should use the filenames given there. If not, we must use the
									// 8.3 filename given in the short entry.

	fat32_direntry_t *short_entry = NULL;

	int is_long_direntry; // Flag to control the outer loop

	// Loop until we get to the short direntry corresponding to long direntries
	// that we might encounter
	do
	{
		is_long_direntry = FALSE;

		// If we've reached the end, we must get to the next clustah
		if (dir->cluster_buffer_offset + 32 > dir->fs->info.bytes_per_cluster)
		{
			FAT_LOG_PRINTF(debug, "Reached end of cluster %d", (int)dir->active_cluster);
			int ret;
			if ((ret = advance_dir_cluster(dir)) != OK)
			{
				return ret;
			}

			if (dir->cluster_buffer_offset == -1)
			{
				// No more clusters, we're done.
				return OK;
			}
		}

		// Choo choo! Violating strict aliasing rules. Compiler may decide to murder cat.
		fat32_any_direntry_t *direntry =
			(fat32_any_direntry_t *)&dir->cluster_buffer[dir->cluster_buffer_offset];
		dir->cluster_buffer_offset += 32;

		if (direntry->short_entry.filename_83[0] == '\0')
		{
			// Last directory entry in this cluster
			FAT_LOG_PRINTF(debug, "Reached end of direntry in cluster %d", (int)dir->active_cluster);
			int ret;
			if ((ret = advance_dir_cluster(dir)) != OK)
			{
				return ret;
			}

			if (dir->cluster_buffer_offset == -1)
			{
				// No more clusters, we're done.
				return OK;
			}

			// If the previous calls were successful, we'll re-enter the loop
			// with a current_buffer_offset of 0 and a new cluster inside the
			// buffer.
		}
		else if (direntry->short_entry.attributes == 0x0f)
		{
			// 0x0F as attributes field means this is a long direntry.

			is_long_direntry = TRUE;
			seen_long_direntry = TRUE;

			if (pfname < filename_buf)
			{ // Truncated filename in a previous direntry?
				continue;
			}

			// Copy the filename into a temporary buffer for easier handling
			// because its bytes are scattered around the struct
			uint16_t temp_lfn_buf[16];
			uint16_t *pbuf = temp_lfn_buf;
			memset(temp_lfn_buf, 0, sizeof(temp_lfn_buf));

			for (int i = 0; i < 5; i++)
			{
				*pbuf++ = direntry->long_entry.chars_1[i];
			}
			for (int i = 0; i < 6; i++)
			{
				*pbuf++ = direntry->long_entry.chars_2[i];
			}
			for (int i = 0; i < 2; i++)
			{
				*pbuf++ = direntry->long_entry.chars_3[i];
			}

			// Find the length of this buffer. Even if the original string has no
			// null terminator (i.e. its length fills the direntry char fields
			// completely), we have a bit of leeway inside the buffer and we
			// memset'd it to zero, so we're guaranteed to get to it.
			int len = 0;
			for (uint16_t *len_pbuf = temp_lfn_buf; *len_pbuf; len_pbuf++)
			{
				len++;
			}

			for (uint16_t *pbuf_r = temp_lfn_buf + len - 1; pbuf_r >= temp_lfn_buf; pbuf_r--)
			{
				// Truncate 16 bits to only ASCII.
				*pfname-- = (char)(*pbuf_r & 0xff);
				if (pfname < filename_buf)
				{
					// The filename is too big for our buffer.
					FAT_LOG_PUTS(warn, "Long filename is too big for the buffer. Truncating.");
					break;
				}
			}
		}
		else
		{
			short_entry = (fat32_direntry_t *)direntry;
		}
	} while (is_long_direntry);

	// if (short_entry->attributes & static_cast<decltype(short_entry->attributes)>(FAT32Attributes::FAT32_ATTR_DIR))
	// {
	// dir->last_entry_was_dir = TRUE;
	// }

	dir->last_entry_was_dir = has_attribute(short_entry->attributes, FAT32Attributes::FAT32_ATTR_DIR);

	int first_cluster_nr = direntry_full_addr(short_entry);

	// This is needed so that do_open_dir and do_open_file on this fs_dir_t can
	// get to the correct info about the just-read file.
	dir->last_entry_start_cluster = first_cluster_nr;
	dir->last_entry_size_bytes = short_entry->size_bytes;

	memset(dst, 0, sizeof(fat32_entry_t));
	if (seen_long_direntry)
	{
		strncpy(dst->filename, pfname + 1, FAT32_MAX_NAME_LEN);
		dst->filename[FAT32_MAX_NAME_LEN - 1] = '\0';
	}
	else
	{
		filename_83_to_string(reinterpret_cast<char *>(short_entry->filename_83), dst->filename);
	}

	convert_entry(short_entry, dst);

	FAT_LOG_PRINTF(debug, "Read %s '%s'", dst->is_directory ? "dir" : "file", dst->filename);
	*was_written = TRUE;

	return OK;
}

int do_open_directory(fat32_dir_t *source, endpoint_t who)
{
	// 	if (!source->last_entry_was_dir || source->last_entry_start_cluster < 0) {
	// 		return EINVAL;
	// 	}

	// 	int ret = OK;
	// 	fat32_dir_t *handle;
	// 	CREATE_HANDLE(dir, handle);

	// 	char* buf = (char*) malloc(source->fs->info.bytes_per_cluster);
	// 	if (!buf) {
	// 		ret = ENOMEM;
	// 		goto destroy_handle;
	// 	}

	// 	// do_open_directory opens the directory that was last returned from
	// 	// do_read_dir_entry, so we use this memoized cluster number now.
	// 	int cluster_nr = source->last_entry_start_cluster;
	// 	if ((ret = seek_read_cluster(&source->fs->header, &source->fs->info, source->fs->fd, cluster_nr, buf)) != OK) {
	// 		goto dealloc_buffer;
	// 	}

	// 	handle->fs = source->fs;
	// 	handle->active_cluster = cluster_nr;
	// 	handle->cluster_buffer_offset = 0;
	// 	handle->cluster_buffer = buf;

	// 	return handle->nr;

	// dealloc_buffer:
	// 	free(buf);

	// destroy_handle:
	// 	dir_handle_count--;
	// 	dir_handle_next--;

	// 	return ret;
	if (!source->last_entry_was_dir || source->last_entry_start_cluster < 0)
	{
		return EINVAL;
	}

	int ret = OK;
	fat32_dir_t *handle;
	CREATE_HANDLE(dir, handle);

	do
	{
		char *buf = (char *)malloc(source->fs->info.bytes_per_cluster);
		if (!buf)
		{
			ret = ENOMEM;
			// goto destroy_handle;
			break;
		}

		// do_open_directory opens the directory that was last returned from
		// do_read_dir_entry, so we use this memoized cluster number now.
		int cluster_nr = source->last_entry_start_cluster;
		if ((ret = seek_read_cluster(&source->fs->header, &source->fs->info, source->fs->fd, cluster_nr, buf)) != OK)
		{
			// goto dealloc_buffer;
			free(buf);
			break;
		}

		handle->fs = source->fs;
		handle->active_cluster = cluster_nr;
		handle->cluster_buffer_offset = 0;
		handle->cluster_buffer = buf;

		return handle->nr;
	} while (0);

	dir_handle_count--;
	dir_handle_next--;

	return ret;
}

int do_open_file(fat32_dir_t *source, endpoint_t who)
{
	if (source->last_entry_was_dir || source->last_entry_start_cluster < 0)
	{
		return EINVAL;
	}

	fat32_file_t *handle;
	CREATE_HANDLE(file, handle);

	handle->fs = source->fs;
	handle->active_cluster = source->last_entry_start_cluster;
	handle->remaining_size = source->last_entry_size_bytes;

	return handle->nr;
}

int do_read_file_block(fat32_file_t *file, char *buffer, int *len, endpoint_t who)
{
	if (*len < file->fs->info.bytes_per_cluster)
	{
		return EINVAL;
	}

	*len = 0;
	if (file->active_cluster == -1 || file->remaining_size == 0)
	{
		// Means we have reached the end of this file and there are no more
		// clusters. file->remaining_size may be 0 even if file->active_cluster
		// is not -1 in some strange cases where new clusters are preallocated
		// and the file ends exactly on a cluster boundary.
		return OK;
	}

	int ret;
	if ((ret = seek_read_cluster(&file->fs->header, &file->fs->info, file->fs->fd, file->active_cluster, buffer)) != OK)
	{
		return ret;
	}

	if (file->remaining_size < file->fs->info.bytes_per_cluster)
	{
		// The size remaining is less than what we've read, ensure that we don't
		// read anything for this file anymore.
		*len = file->remaining_size;
		file->remaining_size = 0;
		file->active_cluster = -1;
	}
	else
	{
		// The size remaining is
		*len = file->fs->info.bytes_per_cluster;
		file->remaining_size -= file->fs->info.bytes_per_cluster;

		int prev_cluster = file->active_cluster;
		if ((ret = advance_file_cluster(file)) != OK)
		{
			return ret;
		}

		if (file->active_cluster == -1 && file->remaining_size > 0)
		{
			FAT_LOG_PRINTF(warn, "There is no next cluster after %d for file handle %d, but there are %d "
								 "bytes remaining to read",
						   prev_cluster, file->nr, file->remaining_size);
		}
	}

	return OK;
}

int do_close_file(fat32_file_t *file, endpoint_t who)
{
	DESTROY_HANDLE(file, file);
	FAT_LOG_PRINTF(debug, "destroying file %d", file->nr);
	for (int i = 0; i < file_handle_count; i++)
	{
		FAT_LOG_PRINTF(debug, "handle = %d", file_handles[i].nr);
	}

	return OK;
}

int do_close_directory(fat32_dir_t *dir, endpoint_t who)
{
	free(dir->cluster_buffer);
	DESTROY_HANDLE(dir, dir);

	return OK;
}

int do_close_fs(fat32_fs_t *fs, endpoint_t who)
{
	close(fs->fd);
	DESTROY_HANDLE(fs, fs);

	return OK;
}
