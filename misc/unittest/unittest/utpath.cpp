#include "common.h"
#include "utpath.h"
#include <vector>
#include <string.h>
#include <malloc.h>
#include <assert.h>
namespace unittest {
#pragma pack(push,1)
struct s_cache
{
	s_cache* prev;
	s_cache* next;
	uint len;
	char sz[1];
};
#pragma pack(pop)
inline s_cache* init_s_cache(const char* s)
{
	uint len=strlen(s);
	s_cache* sc=(s_cache*)malloc(sizeof(s_cache)+len);
	strcpy(sc->sz,s);
	sc->len=len;
	sc->next=sc->prev=sc;
	return sc;
}
inline s_cache* init_s_cache(const char* s,uint len)
{
	s_cache* sc=(s_cache*)malloc(sizeof(s_cache)+len);
	memcpy(sc->sz,s,len);
	sc->sz[len]=0;
	sc->len=len;
	sc->next=sc->prev=sc;
	return sc;
}
inline void insert_s_cache(s_cache* sc,s_cache* insert_before)
{
	sc->prev=insert_before->prev;
	sc->next=insert_before;
	insert_before->prev->next=sc;
	insert_before->prev=sc;
}
inline void insert_s_cache(s_cache* sca,s_cache* scb,s_cache* insert_before)
{
	sca->prev=insert_before->prev;
	scb->next=insert_before;
	insert_before->prev->next=sca;
	insert_before->prev=scb;
}
inline void remove_s_cache(s_cache* sc)
{
	sc->prev->next=sc->next;
	sc->next->prev=sc->prev;
	sc->prev=sc->next=sc;
}
inline void remove_s_cache(s_cache* sca,s_cache* scb)
{
	sca->prev->next=scb->next;
	scb->next->prev=sca->prev;
	sca->prev=scb;
	scb->next=sca;
}
class path_it_base
{
public:
	path_it_base():ref_cnt(1){}
	path_it_base(const path_it_base& iter):ref_cnt(1){}
	virtual ~path_it_base(){}
	void AddRef(){ref_cnt++;}
	void Release()
	{
		if((--ref_cnt)==0)
			delete this;
	}
	path_it_base* slit()
	{
		if(ref_cnt==1)
			return this;
		else
		{
			Release();
			return clone();
		}
	}
	virtual void operator++(int)=0;
	virtual void operator++()=0;
	virtual void operator--(int)=0;
	virtual void operator--()=0;
	virtual bool operator==(const path_it_base& other) const=0;
	virtual const char* operator*() const=0;
	virtual path_it_base* clone()=0;
private:
	mutable int ref_cnt;
};
class path_it_stl:public path_it_base
{
	friend class path_cache_stl;
public:
	path_it_stl(vector<string>::iterator& it,path_cache_stl* v):iter(it),vhost(v){}
	virtual void operator++(int)
	{
		iter++;
	}
	virtual void operator++()
	{
		++iter;
	}
	virtual void operator--(int)
	{
		iter--;
	}
	virtual void operator--()
	{
		--iter;
	}
	virtual bool operator==(const path_it_base& other) const
	{
		assert(dynamic_cast<const path_it_stl*>(&other));
		const path_it_stl* it=(path_it_stl*)&other;
		return iter==it->iter&&vhost==it->vhost;
	}
	virtual const char* operator*() const
	{
		return iter->c_str();
	}
	virtual path_it_base* clone()
	{
		return new path_it_stl(*this);
	}
private:
	vector<string>::iterator iter;
	path_cache_stl* vhost;
};
class path_it_c:public path_it_base
{
	friend class path_cache_c;
public:
	path_it_c(s_cache* it,path_cache_c* v):iter(it),vhost(v){}
	virtual void operator++(int)
	{
		iter=iter->next;
	}
	virtual void operator++()
	{
		iter=iter->next;
	}
	virtual void operator--(int)
	{
		iter=iter->prev;
	}
	virtual void operator--()
	{
		iter=iter->prev;
	}
	virtual bool operator==(const path_it_base& other) const
	{
		assert(dynamic_cast<const path_it_c*>(&other));
		const path_it_c* it=(path_it_c*)&other;
		return iter==it->iter&&vhost==it->vhost;
	}
	virtual const char* operator*() const
	{
		return iter->sz;
	}
	virtual path_it_base* clone()
	{
		return new path_it_c(*this);
	}
private:
	s_cache* iter;
	path_cache_c* vhost;
};
path_cache::iterator::iterator(const iterator& it)
{
	((path_it_base*)it.iter)->AddRef();
	iter=it.iter;
}
path_cache::iterator::~iterator()
{
	((path_it_base*)iter)->Release();
}
path_cache::iterator& path_cache::iterator::operator=(const path_cache::iterator& other)
{
	((path_it_base*)other.iter)->AddRef();
	((path_it_base*)iter)->Release();
	iter=other.iter;
	return *this;
}
path_cache::iterator path_cache::iterator::operator++(int)
{
	iterator it;
	it.iter=((path_it_base*)iter)->clone();
	(*(path_it_base*)iter)++;
	return it;
}
path_cache::iterator& path_cache::iterator::operator++()
{
	iter=((path_it_base*)iter)->slit();
	++(*(path_it_base*)iter);
	return *this;
}
path_cache::iterator path_cache::iterator::operator--(int)
{
	iterator it;
	it.iter=((path_it_base*)iter)->clone();
	(*(path_it_base*)iter)--;
	return it;
}
path_cache::iterator& path_cache::iterator::operator--()
{
	iter=((path_it_base*)iter)->slit();
	--(*(path_it_base*)iter);
	return *this;
}
bool path_cache::iterator::operator==(const path_cache::iterator& other) const
{
	return *(path_it_base*)iter==*(path_it_base*)other.iter;
}
bool path_cache::iterator::operator!=(const path_cache::iterator& other) const
{
	return !(*(path_it_base*)iter==*(path_it_base*)other.iter);
}
const char* path_cache::iterator::operator*() const
{
	return **(path_it_base*)iter;
}
class path_cache_base
{
	friend void merge_path(string& path, const path_cache& split, char dsym);
public:
	path_cache_base():total_len(0){}
	virtual ~path_cache_base(){}
	virtual void clear()=0;
	virtual uint size() const=0;
	virtual bool empty() const=0;
	virtual void push_back(const char* str)=0;
	virtual void push_back(const char* str,uint len)=0;
	virtual void push_back(path_it_base* it)=0;
	virtual const char* back() const=0;
	virtual path_it_base* begin()=0;
	virtual path_it_base* end()=0;
	virtual void insert(path_it_base* before,path_it_base* start,path_it_base* end)=0;
	virtual void erase(path_it_base* it)=0;
	virtual void erase(path_it_base* start,path_it_base* end)=0;
protected:
	mutable uint total_len;
};
class path_cache_stl:public path_cache_base
{
public:
	virtual void clear()
	{
		vs.clear();
		total_len=0;
	}
	virtual uint size() const
	{
		return vs.size();
	}
	virtual bool empty() const
	{
		return vs.empty();
	}
	virtual void push_back(const char* str)
	{
		vs.push_back(str);
		total_len+=vs.back().size();
	}
	virtual void push_back(const char* str,uint len)
	{
		char* buf=new char[len+1];
		memcpy(buf,str,len);
		buf[len]=0;
		vs.push_back(buf);
		delete[] buf;
		total_len+=len;
	}
	virtual void push_back(path_it_base* it)
	{
		assert(dynamic_cast<path_it_stl*>(it));
		path_it_stl* pit=(path_it_stl*)it;
		vector<string>::iterator oldit=pit->iter;
		vs.push_back(*pit->iter);
		pit->iter=oldit+1;
		total_len+=vs.back().size();
	}
	virtual const char* back() const
	{
		return vs.back().c_str();
	}
	virtual path_it_base* begin()
	{
		return new path_it_stl(vs.begin(),this);
	}
	virtual path_it_base* end()
	{
		return new path_it_stl(vs.end(),this);
	}
	virtual void insert(path_it_base* before,path_it_base* start,path_it_base* end)
	{
		assert(dynamic_cast<path_it_stl*>(before));
		assert(dynamic_cast<path_it_stl*>(start));
		assert(dynamic_cast<path_it_stl*>(end));
		path_it_stl *pb=(path_it_stl*)before,
			*ps=(path_it_stl*)start,
			*pe=(path_it_stl*)end;
		vs.insert(pb->iter,ps->iter,pe->iter);
		total_len+=ps->vhost->total_len;
	}
	virtual void erase(path_it_base* it)
	{
		assert(dynamic_cast<path_it_stl*>(it));
		path_it_stl* pit=(path_it_stl*)it;
		vs.erase(pit->iter);
	}
	virtual void erase(path_it_base* start,path_it_base* end)
	{
		assert(dynamic_cast<path_it_stl*>(start));
		assert(dynamic_cast<path_it_stl*>(end));
		path_it_stl *ps=(path_it_stl*)start,
			*pe=(path_it_stl*)end;
		vs.erase(ps->iter,pe->iter);
	}
private:
	vector<string> vs;
};
class path_cache_c:public path_cache_base
{
public:
	path_cache_c()
	{
		first.next=&last;
		first.prev=NULL;
		first.len=0;
		first.sz[0]=0;
		last.next=NULL;
		last.prev=&first;
		last.len=0;
		last.sz[0]=0;
		csize=0;
	}
	~path_cache_c()
	{
		path_cache_c::clear();
	}
	virtual void clear()
	{
		s_cache* tmp;
		for(s_cache* ps=first.next;ps!=&last;)
		{
			tmp=ps;
			ps=ps->next;
			free(tmp);
		}
		first.next=&last,last.prev=&first;
		csize=0;
		total_len=0;
	}
	virtual uint size() const
	{
		if(first.next==&last)
		{
			csize=0;
			total_len=0;
		}
		return csize;
	}
	virtual bool empty() const
	{
		if(first.next==&last)
		{
			csize=0;
			total_len=0;
		}
		return first.next==&last;
	}
	virtual void push_back(const char* str)
	{
		s_cache* sc=init_s_cache(str);
		insert_s_cache(sc,&last);
		csize++;
		total_len+=sc->len;
	}
	virtual void push_back(const char* str,uint len)
	{
		s_cache* sc=init_s_cache(str,len);
		insert_s_cache(sc,&last);
		csize++;
		total_len+=len;
	}
	virtual void push_back(path_it_base* it)
	{
		assert(dynamic_cast<path_it_c*>(it));
		path_it_c* pit=(path_it_c*)it;
		s_cache* itn=pit->iter->next;
		remove_s_cache(pit->iter);
		insert_s_cache(pit->iter,&last);
		csize++;
		total_len+=pit->iter->len;
		pit->iter=itn;
	}
	virtual const char* back() const
	{
		return last.prev->sz;
	}
	virtual path_it_base* begin()
	{
		return new path_it_c(first.next,this);
	}
	virtual path_it_base* end()
	{
		return new path_it_c(&last,this);
	}
	virtual void insert(path_it_base* before,path_it_base* start,path_it_base* end)
	{
		assert(dynamic_cast<path_it_c*>(before));
		assert(dynamic_cast<path_it_c*>(start));
		assert(dynamic_cast<path_it_c*>(end));
		path_it_c *pb=(path_it_c*)before,
			*ps=(path_it_c*)start,
			*pe=(path_it_c*)end;
		if(ps->iter==pe->iter)
			return;
		uint acc_size=ps->vhost->csize,
			acc_total_len=ps->vhost->total_len;
		s_cache* prev=pe->iter->prev;
		remove_s_cache(ps->iter,prev);
		insert_s_cache(ps->iter,prev,pb->iter);
		csize+=acc_size;
		total_len+=acc_total_len;
		ps->iter=pe->iter;
	}
	virtual void erase(path_it_base* it)
	{
		assert(dynamic_cast<path_it_c*>(it));
		path_it_c* pit=(path_it_c*)it;
		s_cache* itn=pit->iter->next;
		uint len=pit->iter->len;
		remove_s_cache(pit->iter);
		free(pit->iter);
		pit->iter=itn;
		csize--;
		total_len-=len;
	}
	virtual void erase(path_it_base* start,path_it_base* end)
	{
		assert(dynamic_cast<path_it_c*>(start));
		assert(dynamic_cast<path_it_c*>(end));
		path_it_c *ps=(path_it_c*)start,
			*pe=(path_it_c*)end;
		if(ps->iter==pe->iter)
			return;
		s_cache *begin=ps->iter,
			*prev=pe->iter->prev,*tmp;
		remove_s_cache(begin,prev);
		do
		{
			tmp=begin;
			begin=begin->next;
			free(tmp);
		}while(begin!=prev);
		ps->iter=pe->iter;
	}
private:
	s_cache first;
	s_cache last;
	mutable uint csize;
};
path_cache::path_cache(path_cache_type t)
{
	pcache=NULL;
	path_cache_base* base=NULL;
	switch(t)
	{
	case PCT_STL_STYLE:
		base=new path_cache_stl;
		break;
	case PCT_C_STYLE:
		base=new path_cache_c;
		break;
	default:
		assert(false);
		break;
	}
	pcache=base;
}
path_cache::~path_cache()
{
	if(pcache!=NULL)
		delete (path_cache_base*)pcache;
}
void path_cache::clear()
{
	((path_cache_base*)pcache)->clear();
}
bool path_cache::empty() const
{
	return ((path_cache_base*)pcache)->empty();
}
void path_cache::push_back(const char* str)
{
	((path_cache_base*)pcache)->push_back(str);
}
void path_cache::push_back(const char* str, uint len)
{
	((path_cache_base*)pcache)->push_back(str,len);
}
void path_cache::push_back(const iterator& it)
{
	((path_cache_base*)pcache)->push_back((path_it_base*)it.iter);
}
void path_cache::pop_back(path_cache* cache)
{
	path_it_base* it=((path_cache_base*)pcache)->end();
	--*it;
	if(cache==NULL)
		((path_cache_base*)pcache)->erase(it);
	else
		((path_cache_base*)cache->pcache)->push_back(it);
	it->Release();
}
const char* path_cache::back() const
{
	return ((path_cache_base*)pcache)->back();
}
path_cache::iterator path_cache::begin() const
{
	iterator it;
	it.iter=((path_cache_base*)pcache)->begin();
	return it;
}
path_cache::iterator path_cache::end() const
{
	iterator it;
	it.iter=((path_cache_base*)pcache)->end();
	return it;
}
void path_cache::insert(const iterator& before,const iterator& start,const iterator& end)
{
	((path_cache_base*)pcache)->insert((path_it_base*)before.iter,(path_it_base*)start.iter,(path_it_base*)end.iter);
}
void path_cache::erase(const iterator& it)
{
	((path_cache_base*)pcache)->erase((path_it_base*)it.iter);
}
void path_cache::erase(const iterator& start,const iterator& end)
{
	((path_cache_base*)pcache)->erase((path_it_base*)start.iter,(path_it_base*)end.iter);
}
void split_path(const string& path, path_cache& split, char dsym)
{
	split.clear();
	int pos=0;
	int nextpos=0;
	while((nextpos=path.find(dsym,pos))!=string::npos)
	{
		if(nextpos>pos)
			split.push_back(path.c_str()+pos,nextpos-pos);
		pos=nextpos+1;
	}
	if(pos<(int)path.length())
		split.push_back(path.c_str()+pos);
}
inline bool add_path_component(path_cache& path, path_cache::iterator& comp)
{
	const char* s=*comp;
	if(strcmp(s,".")==0)
	{
		++comp;
		return true;
	}
	else if(strcmp(s,"..")==0)
	{
		if(!path.empty())
		{
			path.pop_back();
			++comp;
			return true;
		}
		return false;
	}
	else
	{
		path.push_back(comp);
		return true;
	}
}
inline int max(int a,int b)
{
	return a>b?a:b;
}
void merge_path(string& path, const path_cache& split, char dsym)
{
	path.clear();
	uint total_len=((path_cache_base*)split.pcache)->total_len;
	int size=((path_cache_base*)split.pcache)->size();
	path.reserve(max(size-1,0)+total_len+16);
	bool start=true;
	path_cache::iterator end=split.end();
	for(path_cache::iterator it=split.begin();it!=end;++it)
	{
		if(!start)
			path+=dsym;
		else
			start=false;
		path+=*it;
	}
}
int get_direct_path(path_cache& direct, const path_cache& indirect)
{
	direct.clear();
	path_cache::iterator end=indirect.end();
	for(path_cache::iterator it=indirect.begin();it!=end;)
	{
		if(!add_path_component(direct, it))
		{
			direct.clear();
			return ERR_INVALID_PATH;
		}
	}
	return 0;
}
int get_absolute_path(const string& cur_dir, const path_cache& relative_path, path_cache& absolute_path, char dsym, path_cache_type type)
{
	path_cache cur_dir_split(type);
	split_path(cur_dir,cur_dir_split,dsym);
	cur_dir_split.insert(cur_dir_split.end(),relative_path.begin(),relative_path.end());
	return get_direct_path(absolute_path, cur_dir_split);
}
int get_absolute_path(const string& cur_dir, const string& relative_path, path_cache& absolute_path, char dsym, path_cache_type type)
{
	path_cache relative_path_split(type);
	split_path(relative_path, relative_path_split, dsym);
	return get_absolute_path(cur_dir, relative_path_split, absolute_path, dsym, type);
}
int get_absolute_path(const string& cur_dir, const string& relative_path, string& absolute_path, int (*is_absolute_path)(char* path, char dsym), char dsym, path_cache_type type)
{
	int ret=0;
	path_cache array_absolute_path(type);
	if(is_absolute_path!=NULL&&is_absolute_path((char*)relative_path.c_str(),dsym))
	{
		if(0!=(ret=get_absolute_path(relative_path, "", array_absolute_path, dsym, type)))
			return ret;
	}
	else
	{
		if(0!=(ret=get_absolute_path(cur_dir, relative_path, array_absolute_path, dsym, type)))
			return ret;
	}
	merge_path(absolute_path, array_absolute_path, dsym);
	return 0;
}
void concat_path(const string& path1, const string& path2, string& merge, char dsym, path_cache_type type)
{
	path_cache path_array1(type), path_array2(type);
	split_path(path1,path_array1,dsym);
	split_path(path2,path_array2,dsym);
	path_array1.insert(path_array1.end(),path_array2.begin(),path_array2.end());
	merge_path(merge, path_array1,dsym);
}
bool is_subpath(const string& relative_path1, const string& relative_path2, char dsym)
{
	return relative_path1.size()>relative_path2.size()
		&&memcmp(relative_path1.c_str(),relative_path2.c_str(),(uint)relative_path2.size())
		&&relative_path1[relative_path2.size()]==dsym;
}
} //namespace unittest