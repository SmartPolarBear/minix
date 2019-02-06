#ifndef MINIX_SERVERS_FAT32_PROTO_H_
#define MINIX_SERVERS_FAT32_PROTO_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <minix/log.h>
#include <minix/ipc.h>
#include <minix/com.h>

#ifdef __cplusplus
}
#endif

#include "fat32.h"


#define FAT_LOG_PRINTF(level, fmt, ...)                       \
	do                                                        \
	{                                                         \
		char _fat32_logbuf[4096];                             \
		snprintf(_fat32_logbuf, 4096, fmt "\n", __VA_ARGS__); \
		log_##level(&fat32_syslog, _fat32_logbuf);            \
	} while (0)

#define FAT_LOG_PUTS(level, str)              \
	do                                        \
	{                                         \
		log_##level(&fat32_syslog, str "\n"); \
	} while (0)

static struct log fat32_syslog = {"fat32", LEVEL_INFO, default_log};
// ={
// 	.name = "fat32",
// 	.log_level = LEVEL_INFO,
// 	.log_func = default_log};

/* Data structures. */

typedef struct fat32_request_t
{
	endpoint_t source;
	int type;
} fat32_request_t;

/* main.c */
extern fat32_fs_t fs_handles[FAT32_MAX_HANDLES];
extern int fs_handle_count;
extern int fs_handle_next;

extern fat32_dir_t dir_handles[FAT32_MAX_HANDLES];
extern int dir_handle_count;
extern int dir_handle_next;

extern fat32_file_t file_handles[FAT32_MAX_HANDLES];
extern int file_handle_count;
extern int file_handle_next;

int main(int argc, char **argv);
void reply(endpoint_t destination, message *msg);
int wait_request(message *msg, fat32_request_t *req);

/* requests.c */

fat32_fs_t *find_fs_handle(int h);
fat32_dir_t *find_dir_handle(int h);
fat32_file_t *find_file_handle(int h);

/* Opens a FAT32 filesystem. */
int do_open_fs(const char *device, endpoint_t who);

/* Opens the root directory of the filesystem for listing. */
int do_open_root_directory(fat32_fs_t *fs, endpoint_t who);

/* Opens the item that was last returned by do_read_dir_entry for a given
 * parent directory, if that item is a directory. */
int do_open_directory(fat32_dir_t *parent, endpoint_t who);

/* Opens the item that was last returned by do_read_dir_entry for a given
 * parent directory, if that item is a file. */
int do_open_file(fat32_dir_t *parent, endpoint_t who);

/* Reads the next block of a file. The buffer must be at least
 * file->fs->info.bytes_per_cluster bytes long. */
int do_read_file_block(fat32_file_t *file, char *buffer, int *len, endpoint_t who);

/* Reads the next directory entry for tis directory. Writes TRUE to *was_written
 * if anything was written to *dst, and FALSE otherwise. The caller may assume
 * that when the call succeeds with *was_written == FALSE, there are no more
 * directory entries in the given directory. */
int do_read_dir_entry(fat32_dir_t *dir, fat32_entry_t *dst, int *was_written, endpoint_t who);

/* Closes a previously open file handle. */
int do_close_file(fat32_file_t *file, endpoint_t who);

/* Closes a previously open directory handle. */
int do_close_directory(fat32_dir_t *dir, endpoint_t who);

/* Closes a previously open FAT32 filesystem handle. This closes the block device
 * backing the handle. */
int do_close_fs(fat32_fs_t *fs, endpoint_t who);

/* Advances the given directory handle one cluster forward in the cluster chain,
 * by setting its active_cluster. It also reads the cluster contents into the
 * directory's cluster buffer. */
int advance_dir_cluster(fat32_dir_t *dir);

/* Advances the given file handle one cluster forward in the cluster chain,
 * likewise. */
int advance_file_cluster(fat32_file_t *file);

#endif