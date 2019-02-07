#include "inc.h"
#include "avl.h"
#include <minix/endpoint.h>
constexpr int Reversion=1;

fat32_fs_t fs_handles[FAT32_MAX_HANDLES];
int fs_handle_count;
int fs_handle_next;

fat32_dir_t dir_handles[FAT32_MAX_HANDLES];
int dir_handle_count;
int dir_handle_next;

fat32_file_t file_handles[FAT32_MAX_HANDLES];
int file_handle_count;
int file_handle_next;

avl_tree_t *fs_tree = nullptr;
avl_tree_t *dir_tree = nullptr;
avl_tree_t *file_tree = nullptr;

/* SEF functions and variables. */
void sef_local_startup(void);

int main(int argc, char **argv)
{
	env_setargs(argc, argv);
	sef_local_startup();

	printf("Load the FAT32 filesystem service (Ver.%d).\n",Reversion);

	fs_tree = avl_create();
	dir_tree = avl_create();
	file_tree = avl_create();

	printf("FAT32: create tables.\n");

	for (;;)
	{
		fat32_entry_t entry;
		fat32_fs_t *fs;
		fat32_dir_t *dir;
		fat32_file_t *file;
		fat32_request_t req;
		int was_written;
		void *dst_addr;
		char *local_buf;
		int local_len;
		message m;
		int result;

		if (wait_request(&m, &req) != OK)
		{
			result = EINVAL;
		}
		else
			switch (req.type)
			{
			case FAT32_OPEN_FS:
				result = do_open_fs(reinterpret_cast<const char *>(m.m_fat32_open_fs.device), m.m_source);
				if (result >= 0)
				{
					m.m_fat32_io_handle.handle = result;
					result = OK;
				}
				break;

			case FAT32_OPEN_ROOTDIR:
				fs = find_fs_handle(m.m_fat32_io_handle.handle);
				if (!fs)
				{
					result = EINVAL;
					break;
				}

				if (fs->opened_by != m.m_source)
				{
					result = EPERM;
					break;
				}

				result = do_open_root_directory(fs, m.m_source);
				if (result >= 0)
				{
					m.m_fat32_io_handle.handle = result;
					result = OK;
				}
				break;

			case FAT32_OPEN_DIR:
				dir = find_dir_handle(m.m_fat32_io_handle.handle);
				if (!dir)
				{
					result = EINVAL;
					break;
				}

				if (dir->fs->opened_by != m.m_source)
				{
					result = EPERM;
					break;
				}

				result = do_open_directory(dir, m.m_source);
				if (result >= 0)
				{
					m.m_fat32_io_handle.handle = result;
					result = OK;
				}
				break;

			case FAT32_OPEN_FILE:
				dir = find_dir_handle(m.m_fat32_io_handle.handle);
				if (!dir)
				{
					result = EINVAL;
					break;
				}

				if (dir->fs->opened_by != m.m_source)
				{
					result = EPERM;
					break;
				}

				result = do_open_file(dir, m.m_source);
				if (result >= 0)
				{
					m.m_fat32_io_handle.handle = result;
					result = OK;
				}
				break;

			case FAT32_READ_DIR_ENTRY:
				dir = find_dir_handle(m.m_fat32_read_direntry.handle);
				m.m_fat32_ret.ret = 0;
				if (!dir)
				{
					result = EINVAL;
					break;
				}

				if (dir->fs->opened_by != m.m_source)
				{
					result = EPERM;
					break;
				}

				if ((result = do_read_dir_entry(dir, &entry, &was_written, m.m_source)) != OK)
				{
					break;
				}

				if (!was_written)
				{
					result = OK;
					break;
				}

				dst_addr = m.m_fat32_read_direntry.dest;
				if ((result = sys_vircopy(FAT32_PROC_NR, (vir_bytes)&entry,
										  m.m_source, (vir_bytes)dst_addr, sizeof(fat32_entry_t), 0)) != OK)
				{
					break;
				}

				// Return the buffer size for this directory entry to be
				// read.
				m.m_fat32_ret.ret = dir->fs->info.bytes_per_cluster;
				break;

			case FAT32_READ_FILE_BLOCK:
				file = find_file_handle(m.m_fat32_read_block.handle);
				m.m_fat32_ret.ret = 0;
				if (!file)
				{
					result = EINVAL;
					break;
				}

				if (file->fs->opened_by != m.m_source)
				{
					result = EPERM;
					break;
				}

				dst_addr = m.m_fat32_read_block.buf_ptr;
				local_len = m.m_fat32_read_block.buf_size;

				if (local_len < file->fs->info.bytes_per_cluster)
				{
					result = EINVAL;
					break;
				}

				// Allocating a new buffer every time is criminally wasteful of
				// resources. This should be fixed one day.
				if ((local_buf = reinterpret_cast<char *>(malloc(file->fs->info.bytes_per_cluster))) == NULL)
				{
					result = ENOMEM;
					break;
				}

				if ((result = do_read_file_block(file, local_buf, &local_len, m.m_source)) != OK)
				{
					free(local_buf);
					break;
				}

				if ((result = sys_vircopy(FAT32_PROC_NR, (vir_bytes)local_buf, m.m_source,
										  (vir_bytes)dst_addr, file->fs->info.bytes_per_cluster, 0)) != OK)
				{
					free(local_buf);
					break;
				}

				m.m_fat32_ret.ret = local_len;

				free(local_buf);
				break;

			case FAT32_CLOSE_FILE:
				file = find_file_handle(m.m_fat32_io_handle.handle);
				if (!file)
				{
					result = EINVAL;
					break;
				}

				if (file->fs->opened_by != m.m_source)
				{
					result = EPERM;
					break;
				}

				result = do_close_file(file, m.m_source);
				break;

			case FAT32_CLOSE_DIR:
				dir = find_dir_handle(m.m_fat32_io_handle.handle);
				if (!dir)
				{
					result = EINVAL;
					break;
				}

				if (dir->fs->opened_by != m.m_source)
				{
					result = EPERM;
					break;
				}

				result = do_close_directory(dir, m.m_source);
				break;

			case FAT32_CLOSE_FS:
				fs = find_fs_handle(m.m_fat32_io_handle.handle);
				if (!fs)
				{
					result = EINVAL;
					break;
				}

				if (fs->opened_by != m.m_source)
				{
					result = EPERM;
					break;
				}

				result = do_close_fs(fs, m.m_source);
				break;

			default:
				result = EINVAL;
				break;
			}

		if (result != EDONTREPLY)
		{
			m.m_type = result;
			reply(req.source, &m);
		}
	}

	avl_destroy(fs_tree);
	avl_destroy(dir_tree);
	avl_destroy(file_tree);

	return OK;
}

int wait_request(message *msg, fat32_request_t *req)
{
	int status = sef_receive(ANY, msg);
	if (OK != status)
	{
		FAT_LOG_PRINTF(warn, "Failed to receive message from pid %d: %d", msg->m_source, status);
		return status;
	}

	req->source = msg->m_source;
	if (msg->m_type < FAT32_BASE || msg->m_type >= FAT32_END)
	{
		FAT_LOG_PRINTF(warn, "Invalid message type %d from pid %d", msg->m_type, msg->m_source);
		return -1;
	}

	req->type = msg->m_type;
	return OK;
}

void reply(endpoint_t destination, message *msg)
{
	int s = ipc_send(destination, msg);
	if (OK != s)
	{
		FAT_LOG_PRINTF(warn, "Unable to send reply to %d: %d", destination, s);
	}
}

int sef_cb_init(int type, sef_init_info_t *info)
{
	return (OK);
}

void sef_local_startup()
{
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	/* Let SEF perform startup. */
	sef_startup();
}
