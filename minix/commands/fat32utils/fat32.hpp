#pragma once
#include <string>
#include <memory>
#include <vector>
#include <ctime>
#include <exception>

#define FAT32_MAX_NAME_LEN			256

namespace fat32 {

	template<typename T>
	class maybe {
	public:
		bool is_some;
		T value;

		maybe() : is_some(false) {
		}

		maybe(const T& value) : is_some(true), value(value) {
		}

		maybe(T&& value) : is_some(true), value(std::move(value)) {
		}
	};

	class exception : public std::exception {
	private:
		std::string str;

	public:
		int ret;
		exception(int _ret) : ret(_ret) {
			str = "Return value: " + std::to_string(ret);
		}

		virtual const char* what() const throw() {
			return str.c_str();
		}
	};

	struct entry {
		char filename[FAT32_MAX_NAME_LEN];
		int is_directory;
		int is_readonly;
		int is_hidden;
		int is_system;

		// Only tm_mon, tm_mday, tm_year, tm_sec, tm_hour and tm_sec
		// are set.
		struct tm creation;

		// Only tm_mon, tm_mday and tm_year set.
		struct tm access;

		// Only tm_mon, tm_mday, tm_year, tm_sec, tm_hour and tm_sec
		// are set.
		struct tm modification;

		int size_bytes;
	};

	class file {
	private:
		friend class dir;
		int handle;
		int buf_size;
		file(int _handle, int _buf_size) : handle(_handle), buf_size(_buf_size) {
		}
	
	public:
		maybe<std::vector<uint8_t>> read_block();
		~file();
	};

	class dir {
	private:
		friend class fs;
		int handle;
		int last_buf_size;
		dir(int _handle) : handle(_handle) {}

	public:
		maybe<entry> next_entry();
		std::unique_ptr<dir> open_subdir();
		std::unique_ptr<file> open_file();
		~dir();
	};

	class fs {
	private:
		int handle;

	public:
		fs(std::string device);
		std::unique_ptr<dir> open_root_dir();
		~fs();
	};
}