#include "fat32.hpp"
#include <iostream>
using namespace std;
using namespace fat32;

constexpr int Reversion = 1;

maybe<pair<entry, unique_ptr<dir>>> find_path(unique_ptr<dir> d, string path)
{
	size_t p = path.find('/');
	string to_find = path;
	string to_pass = "";

	if (p != string::npos)
	{
		to_find = path.substr(0, p);
		to_pass = path.substr(p + 1);
	}

	maybe<entry> e;
	while ((e = d->next_entry()).is_some)
	{
		if (string(e.value.filename) == to_find)
		{
			if (p != string::npos)
			{
				if (e.value.is_directory)
				{
					unique_ptr<dir> subdir = d->open_subdir();
					return find_path(move(subdir), to_pass);
				}
			}
			else
			{
				return maybe<pair<entry, unique_ptr<dir>>>(move(make_pair(e.value, move(d))));
			}
		}
	}

	return maybe<pair<entry, unique_ptr<dir>>>();
}

void out_flag(char c, bool on)
{
	cout << c << '[' << (on ? 'x' : ' ') << "]  ";
}

void out_flag(string flg, bool on)
{
	cout << flg << '[' << (on ? 'x' : ' ') << "]  ";
}

void out_time(struct tm t)
{
	char buf[2048];
	strftime(buf, 2047, "%Y-%m-%d %H:%M:%S", &t);

	cout << buf;
}

void do_stat(string path, fs &f)
{
	unique_ptr<dir> root = f.open_root_dir();
	auto ret = find_path(move(root), path);
	if (ret.is_some)
	{
		entry e = ret.value.first;

		cout << "Attributes:    ";
		out_flag("DIR     ", e.is_directory);
		out_flag("READONLY", e.is_readonly);
		out_flag("HIDE    ", e.is_hidden);
		out_flag("SYS     ", e.is_system);
		cout << endl;

		cout << "File name:     " << e.filename << endl;
		cout << "Size (bytes):  " << e.size_bytes << endl;

		cout << "Date Created:  ";
		out_time(e.creation);
		cout << endl;

		cout << "Date Modified: ";
		out_time(e.modification);
		cout << endl;

		cout << "Date Accessed: ";
		out_time(e.access);
		cout << endl;
	}
	else
	{
		cerr << "Fatal: Path not found." << endl;
	}
}

void do_ls(string path, fs &f)
{
	unique_ptr<dir> root = f.open_root_dir();
	if (path.empty())
	{
		maybe<entry> e2;
		while ((e2 = root->next_entry()).is_some)
		{
			cout << '[' << (e2.value.is_directory ? "DIR" : "FILE") << "] " << e2.value.filename << endl;
		}

		return;
	}

	auto ret = find_path(move(root), path);
	if (ret.is_some)
	{
		entry e = ret.value.first;
		if (!e.is_directory)
		{
			cerr << "Fatal: " << path << " is not a directory." << endl;
			return;
		}

		unique_ptr<dir> subdir = ret.value.second->open_subdir();
		maybe<entry> e2;
		while ((e2 = subdir->next_entry()).is_some)
		{
			cout << '[' << (e2.value.is_directory ? "DIR" : "FILE") << "] " << e2.value.filename << endl;
		}
	}
	else
	{
		cerr << "Fatal: Path not found." << endl;
	}
}

void do_cat(string path, fs &f)
{
	unique_ptr<dir> root = f.open_root_dir();
	auto ret = find_path(move(root), path);
	if (ret.is_some)
	{
		if (ret.value.first.is_directory)
		{
			cerr << "Fatal: " << ret.value.first.filename << " is a directory." << endl;
			return;
		}

		unique_ptr<file> fp = ret.value.second->open_file();
		maybe<vector<uint8_t>> e2;
		while ((e2 = fp->read_block()).is_some)
		{
			fwrite(&e2.value[0], e2.value.size(), 1, stdout);
		}
	}
	else
	{
		cerr << "Fatal: Path not found." << endl;
	}
}

void print_tree(unique_ptr<dir> d, int level)
{
	maybe<entry> e;
	while ((e = d->next_entry()).is_some)
	{
		for (int i = 0; i < level * 4; i++)
		{
			cout << ((i % 4 == 0) ? '|' : '-');
		}

		cout << (level == 0 ? '|' : ' ') << e.value.filename << endl;
		if (e.value.is_directory && e.value.filename[0] != '.')
		{
			unique_ptr<dir> subdir = d->open_subdir();
			print_tree(move(subdir), level + 1);
		}
	}
}

void do_tree(string path, fs &f)
{
	unique_ptr<dir> root = f.open_root_dir();
	if (path.empty())
	{
		print_tree(move(root), 0);
	}
	else
	{
		auto ret = find_path(move(root), path);
		if (ret.is_some)
		{
			entry e = ret.value.first;
			if (!e.is_directory)
			{
				cerr << "Fatal: " << path << " is not a directory." << endl;
				return;
			}

			unique_ptr<dir> subdir = ret.value.second->open_subdir();
			print_tree(move(subdir), 0);
		}
		else
		{
			cerr << "Fatal: Path not found." << endl;
		}
	}
}

int main(int argc, char **argv)
{
	cout << "FAT32Utils Version " << Reversion << endl;
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <device/image>\n", argv[0]);
		return -1;
	}

	try
	{
		string device_name(argv[1]);
		fs my_fs(device_name);

		while (true)
		{
			if (feof(stdin))
			{
				break;
			}

			string input;
			cerr << "FAT32Utils $ ";
			getline(cin, input);

			if (input.empty())
			{
				continue;
			}

			if (input == "exit")
			{
				break;
			}

			size_t space = input.find(' ');
			if (space == string::npos)
			{
				cerr << "Fatal: Unrecognized command. \n\nCommands: stat ls cat tree exit" << endl;
				continue;
			}

			string command = input.substr(0, space);
			string param = input.substr(space + 1);
			if (param[0] != '/')
			{
				cerr << "Fatal:Only absolute path, starting with '\', is allowed." << endl;
				continue;
			}

			param = param.substr(1);

			try
			{
				if (command == "stat")
				{
					do_stat(param, my_fs);
				}
				else if (command == "ls")
				{
					do_ls(param, my_fs);
				}
				else if (command == "cat")
				{
					do_cat(param, my_fs);
				}
				else if (command == "tree")
				{
					do_tree(param, my_fs);
				}
				else
				{
					cerr << "Fatal: Unrecognized command. \n\nCommands: stat ls cat tree exit" << endl;
				}
			}
			catch (fat32::exception &e)
			{
				cerr << "Error: " << e.what() << endl;
			}
		}
	}
	catch (fat32::exception &e)
	{
		cerr << "Fatal: Exception occurred while initializing: " << e.what() << endl;
	}

	return 0;
}