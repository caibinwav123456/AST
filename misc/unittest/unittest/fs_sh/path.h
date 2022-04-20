#ifndef _PATH_H_
#define _PATH_H_
#include "dir_symbol.h"
#include <string>
using namespace std;
enum path_cache_type
{
	PCT_STL_STYLE,
	PCT_C_STYLE,
};
class DLL path_cache
{
	friend DLLAPI(void) merge_path(string& path, const path_cache& split, char dsym);
public:
	class DLL iterator
	{
		friend class DLL path_cache;
	private:
		iterator(){};
	public:
		iterator(const iterator& it);
		~iterator();
		iterator& operator=(const iterator& other);
		iterator operator++(int);
		iterator& operator++();
		iterator operator--(int);
		iterator& operator--();
		bool operator==(const iterator& other) const;
		bool operator!=(const iterator& other) const;
		const char* operator*() const;
	private:
		void* iter;
	};
	path_cache(path_cache_type t=PCT_C_STYLE);
private:
	path_cache(const path_cache& cache){};
	path_cache& operator=(const path_cache& other){return *this;}
public:
	~path_cache();
	void clear();
	bool empty() const;
	void push_back(const char* str);
	void push_back(const char* str,uint len);
	void push_back(const iterator& it);
	void pop_back();
	const char* back() const;
	const char* front() const;
	iterator begin() const;
	iterator end() const;
	void insert(const iterator& before,const iterator& start,const iterator& end);
	void erase(const iterator& it);
	void erase(const iterator& start,const iterator& end);
private:
	void* pcache;
};
DLLAPI(void) split_path(const string& path, path_cache& split, char dsym=_dir_symbol);
DLLAPI(void) merge_path(string& path, const path_cache& split, char dsym=_dir_symbol);
DLLAPI(int)get_direct_path(path_cache& direct, const path_cache& indirect);
int get_absolute_path(const string& cur_dir, const path_cache& relative_path, path_cache& absolute_path, char dsym=_dir_symbol, path_cache_type type=PCT_C_STYLE);
int get_absolute_path(const string& cur_dir, const string& relative_path, path_cache& absolute_path, char dsym=_dir_symbol, path_cache_type type=PCT_C_STYLE);
DLLAPI(int) get_absolute_path(const string& cur_dir, const string& relative_path, string& absolute_path, int (*is_absolute_path)(char* path, char dsym)=NULL, char dsym=_dir_symbol, path_cache_type type=PCT_C_STYLE);
DLLAPI(void) concat_path(const string& path1, const string& path2, string& merge, char dsym=_dir_symbol, path_cache_type type=PCT_C_STYLE);
DLLAPI(bool) is_subpath(const string& relative_path1, const string& relative_path2, char dsym=_dir_symbol);
#endif