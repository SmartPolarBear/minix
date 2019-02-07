#include "fat32.hpp"
extern "C"
{
#include <minix/ipc.h>
#include <minix/com.h>
#include <lib.h>
}

using namespace std;

int check_ret(int ret, message *m)
{
	if (ret != 0)
	{
		throw fat32::exception(m->m_type);
	}

	return m->m_type;
}

fat32::fs::fs(string device)
{
	message m;
	memset(&m, 0, sizeof(m));
	if (device.length() >= 56)
	{
		throw fat32::exception(EINVAL);
	}

	strncpy((char *)m.m_fat32_open_fs.device, device.c_str(), 55);
	check_ret(_syscall(FAT32_PROC_NR, FAT32_OPEN_FS, &m), &m);

	handle = m.m_fat32_io_handle.handle;
}

unique_ptr<fat32::dir> fat32::fs::open_root_dir()
{
	message m;
	memset(&m, 0, sizeof(m));
	m.m_fat32_io_handle.handle = handle;

	check_ret(_syscall(FAT32_PROC_NR, FAT32_OPEN_ROOTDIR, &m), &m);
	return unique_ptr<fat32::dir>(new fat32::dir(m.m_fat32_io_handle.handle));
}

fat32::maybe<fat32::entry> fat32::dir::next_entry()
{
	fat32::entry my_entry;

	message m;
	memset(&m, 0, sizeof(m));
	m.m_fat32_read_direntry.handle = handle;
	m.m_fat32_read_direntry.dest = &my_entry;
	check_ret(_syscall(FAT32_PROC_NR, FAT32_READ_DIR_ENTRY, &m), &m);

	if (m.m_fat32_ret.ret == 0)
	{
		return fat32::maybe<fat32::entry>();
	}
	else
	{
		last_buf_size = m.m_fat32_ret.ret;
		return fat32::maybe<fat32::entry>(my_entry);
	}
}

unique_ptr<fat32::dir> fat32::dir::open_subdir()
{
	message m;
	memset(&m, 0, sizeof(m));
	m.m_fat32_io_handle.handle = handle;
	check_ret(_syscall(FAT32_PROC_NR, FAT32_OPEN_DIR, &m), &m);

	return unique_ptr<fat32::dir>(new fat32::dir(m.m_fat32_io_handle.handle));
}

unique_ptr<fat32::file> fat32::dir::open_file()
{
	message m;
	memset(&m, 0, sizeof(m));
	m.m_fat32_io_handle.handle = handle;
	check_ret(_syscall(FAT32_PROC_NR, FAT32_OPEN_FILE, &m), &m);

	return unique_ptr<fat32::file>(new fat32::file(m.m_fat32_io_handle.handle, last_buf_size));
}

fat32::maybe<std::vector<uint8_t>> fat32::file::read_block()
{
	std::vector<uint8_t> buf(buf_size);
	message m;
	memset(&m, 0, sizeof(m));
	m.m_fat32_read_block.handle = handle;
	m.m_fat32_read_block.buf_size = buf_size;
	m.m_fat32_read_block.buf_ptr = &buf[0];
	check_ret(_syscall(FAT32_PROC_NR, FAT32_READ_FILE_BLOCK, &m), &m);

	buf.resize(m.m_fat32_ret.ret);
	if (m.m_fat32_ret.ret == 0)
	{
		return maybe<std::vector<uint8_t>>();
	}
	else
	{
		return maybe<std::vector<uint8_t>>(buf);
	}
}

fat32::file::~file()
{
	message m;
	memset(&m, 0, sizeof(m));
	m.m_fat32_io_handle.handle = handle;
	_syscall(FAT32_PROC_NR, FAT32_CLOSE_DIR, &m);
}

fat32::dir::~dir()
{
	message m;
	memset(&m, 0, sizeof(m));
	m.m_fat32_io_handle.handle = handle;
	_syscall(FAT32_PROC_NR, FAT32_CLOSE_DIR, &m);
}

fat32::fs::~fs()
{
	message m;
	memset(&m, 0, sizeof(m));
	m.m_fat32_io_handle.handle = handle;
	_syscall(FAT32_PROC_NR, FAT32_CLOSE_FS, &m);
}