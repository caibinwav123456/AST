#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string>
#include "common.h"
#include "win.h"
#include "utility.h"
#include "config_val_extern.h"
#include "datetime.h"
#include "Integer64.h"
#include "algor_templ.h"
#include "fsdrv_interface.h"
#include "mutex.h"
#include "pipe.h"
#include "sysfs.h"
using namespace std;
DEFINE_FLOAT_VAL(eg_fl,0);
DEFINE_DOUBLE_VAL(eg_db,0);
DEFINE_STRING_VAL(eg_str,"my");
DEFINE_SIZE_VAL(eg_size,0);
void testfile()
{
	int ret=sys_mkdir("D:\\unit_test\\");
	ret=sys_mkdir("D:\\unit_test\\test1\\test2\\");
	ret=sys_recurse_fcopy("D:\\unit_test\\","D:\\unit_test\\test1\\",NULL);

	void* hFile=sys_fopen("D:\\unit_test\\test1\\test2\\test.txt",FILE_READ|FILE_WRITE|FILE_CREATE_ALWAYS);
	char* str="hello";
	ret=sys_fwrite(hFile,str,strlen(str));
	ret=sys_fflush(hFile);
	sys_fclose(hFile);

	hFile=sys_fopen("D:\\unit_test\\test1\\test2\\test2.txt",FILE_READ|FILE_WRITE|FILE_CREATE_ALWAYS|FILE_NOCACHE);
	str="hellow";
	ret=sys_fwrite(hFile,str,strlen(str));
	ret=sys_fflush(hFile);
	sys_fclose(hFile);

	hFile=sys_fopen("D:\\unit_test\\test1\\test2\\test.txt",FILE_READ|FILE_WRITE|FILE_OPEN_EXISTING);
	char buf[10];
	ret=sys_fread(hFile,buf,6);
	dword size;
	ret=sys_get_file_size(hFile,&size);
	sys_fclose(hFile);

	hFile=sys_fopen("D:\\unit_test\\test1\\test2\\test2.txt",FILE_READ|FILE_WRITE|FILE_OPEN_EXISTING);
	ret=sys_fseek(hFile,20);
	ret=sys_set_file_size(hFile,10);
	ret=sys_get_file_size(hFile,&size);
	sys_fclose(hFile);

	hFile=sys_fopen("D:\\unit_test\\test1\\test2\\test3.txt",FILE_READ|FILE_WRITE|FILE_CREATE_ALWAYS|FILE_NOCACHE);
	str="hello";
	ret=sys_fwrite(hFile,str,strlen(str));
	ret=sys_fseek(hFile,10);
	ret=sys_fwrite(hFile,str,strlen(str));
	ret=sys_get_file_size(hFile,&size);
	ret=sys_fflush(hFile);
	sys_fclose(hFile);

	ret=sys_recurse_fcopy("D:\\unit_test\\","D:\\unit_test2\\",NULL);
	ret=sys_recurse_fdelete("D:\\unit_test\\",NULL);
}
static bool check_instance_exist()
{
	void* hproc=sys_get_process(get_main_info()->manager_exe_file);
	if(!VALID(hproc))
		return false;
	sys_close_process(hproc);
	return true;
}
static RequestResolver reqrslvr;
void test_fs()
{
	int ret=0;
	void* handle=0;
	if(!check_instance_exist())
		return;
	if(0!=(ret=fsc_init(4,2048,NULL,&reqrslvr)))
		return;
	ret=fs_move("sto0:/","sto0:/xyz");
	ret=fs_move("sto1:/disk1/","sto1:");
	ret=fs_delete("sto0:");
	handle=fs_open("/",FILE_CREATE_ALWAYS|FILE_READ|FILE_WRITE);
	handle=fs_open("sto1:",FILE_CREATE_ALWAYS|FILE_READ|FILE_WRITE);
	fsc_exit();
}
void test_read()
{
	void* hFile=sys_fopen("test_read.txt",FILE_READ|FILE_OPEN_EXISTING);
	if(!VALID(hFile))
		return;
	char buf[16];
	memset(buf,0,sizeof(buf));
	uint rdlen=0;
	int ret=sys_fread(hFile,buf,9,&rdlen);
	sys_fclose(hFile);
}
void testtime()
{
	printf("%d\n%d\n",sizeof(DateTime),sizeof(DateTimeWrap));
	DateTime dt;
	sys_get_date_time(&dt);
	CDateTime date(dt),date2;
	assert(date.ValidDate());
	assert(date2.ValidDate());
	date2=date;
	int days=date.ConvertToEpoch();
	date2.ConvertFromEpoch(days);
	assert(date2.ValidDate());
	assert(date==date2);
	for (int i=-365*800;i<=365*800;i++)
	{
		CDateTime date;
		date.ConvertFromEpoch(i);
		assert(date.ValidDate());
		int days=date.ConvertToEpoch();
		assert(days==i);
	}
	CDateTime::SetWorkDay(true);
	CDateTime date3(2019,8,7,21);
	string sd;
	date3.Format(sd,FORMAT_TIME|FORMAT_DATE|FORMAT_WEEKDAY|FORMAT_MILLISEC);
	CDateTime date4=date3+CTimeSpan(0,2);
	CDateTime date5=date3-CTimeSpan(0,22);
	CTimeSpan span=date5-date3;
	assert(span==CTimeSpan(0,22));
	for(int b=0;b<2;b++)
	for(int i=0;i<500;i++)
	{
		CDateTime date(2019,8,7,b?3:21);
		date+=CTimeSpan((uint)i);
		assert(date.ValidDate());
		date.Format(sd,FORMAT_TIME|FORMAT_DATE|FORMAT_WEEKDAY);
		printf("%s\n",sd.c_str());
		for(int b2=0;b2<2;b2++)
		for(int j=0;j<500;j++)
		{
			CTimeSpan span((uint)j,b2?2:22);
			CDateTime t=date+span;
			assert(t.ValidDate());
			CTimeSpan sp=date-t;
			assert(sp==span);
			t=date-span;
			sp=date-t;
			assert(sp==span);
		}
	}
}
void test_int64(int k)
{
	Integer64 int64_1(3*k),int64_2(-5*k);
	UInteger64 uint64_1(2*k),uint64_2(-4*k);
	Integer64 sum=int64_1+int64_2,
		diff=int64_1-int64_2;
	UInteger64 usum=uint64_1+uint64_2,
		udiff=uint64_1-uint64_2;
	bool b1=int64_1==int64_2,
		b2=int64_1!=int64_2,
		b3=int64_1<int64_2,
		b4=int64_1>int64_2;
	bool b01=uint64_1==uint64_2,
		b02=uint64_1!=uint64_2,
		b03=uint64_1<uint64_2,
		b04=uint64_1>uint64_2;
	Integer64 i64(-3);
	UInteger64 u64(-3);
	string str1=FormatI64(i64);
	string str2=FormatI64(u64);
	i64.high=0xff00aa00;
	i64.low=0;
	string str3=FormatI64(i64);
	u64.high=0x80f0a070;
	u64.low=0xff789abc;
	string str4=FormatI64(u64);
}
uint rand_uint32(int neg)
{
	int b=rand()%2;
	int n=b?8:rand()%8;
	uint ret=0;
	for(int i=0;i<n;i++)
	{
		ret|=((rand()&0xf)<<(i*4));
	}
	if(neg<0&&((int)ret)>0)
		ret=(uint)-(int)ret;
	else if(neg>0&&((int)ret)<0)
		ret=(uint)-(int)ret;
	return ret;
}
UInteger64 rand_uint64(int neg)
{
	int low=rand_uint32(0);
	uint high=rand_uint32(neg);
	return UInteger64(low,&high);
}
Integer64 rand_int64(int neg)
{
	int low=rand_uint32(0);
	int high=rand_uint32(neg);
	return Integer64(low,&high);
}
void unused_int64_func()
{
	Integer64 int64_1,int64_2;
	UInteger64 uint64_1,uint64_2;
	Integer64 sum=int64_1+int64_2,
		diff=int64_1-int64_2;
	UInteger64 usum=uint64_1+uint64_2,
		udiff=uint64_1-uint64_2;
	int64_1+=int64_2;
	int64_1-=int64_2;
	uint64_1+=uint64_2;
	uint64_1-=uint64_2;
	string s;
	if(int64_1==int64_2&&int64_1!=int64_2
		&&int64_1<int64_2&&int64_1>int64_2)
	{
		printf("unuesd\n");
	}
	if(uint64_1==uint64_2&&uint64_1!=uint64_2
		&&uint64_1<uint64_2&&uint64_1>uint64_2)
	{
		printf("unuesd\n");
	}
	Mul64(int64_1,int64_2);
	Mul64(uint64_1,uint64_2);
	FormatI64(int64_1);
	FormatI64(uint64_1);
	FormatI64Hex(int64_1);
	FormatI64Hex(uint64_1);
	I64FromDec(s,int64_1);
	I64FromDec(s,uint64_1);
	I64FromHex(s,int64_1);
	I64FromHex(s,uint64_1);
}
void test_i64()
{
	UInteger64 uint1(-1);
	Integer64 int1(-1),int2(1);
	UInteger64 uhmul;
	Integer64 hmul;
	UInteger64 umul=Mul64(uint1,uint1,&uhmul);
	Mul64(uint1,uint1);
	Integer64 mul=Mul64(int1,int1,&hmul);
	Mul64(int1,int1);
	Integer64 mul2=Mul64(int1,int2,&hmul);
	unused_int64_func();
	srand((uint)time(NULL));
	string s;
	for(int i=0;i<16;i++)
	{
		UInteger64 us=rand_uint64(i%2?-1:1),us2;
		Integer64 is=rand_int64(i%2?-1:1),is2;
		s=FormatI64(us);
		assert(I64FromDec(s,us2));
		assert(us==us2);
		s=FormatI64(is);
		assert(I64FromDec(s,is2));
		assert(is==is2);
		s=FormatI64Hex(us);
		assert(I64FromHex(s,us2));
		assert(us==us2);
		s=FormatI64Hex(is);
		assert(I64FromHex(s,is2));
		assert(is==is2);
	}
}
void testBiRing()
{
	BiRingNode<int> node;
	BiRing<int> ring;
	ring.AddNodeToBegin(&node);
	for(BiRing<int>::iterator it=ring.BeginIterate();it;it++)
	{
	}
	for(BiRing<int>::iterator it=ring.BeginReverseIterate();it;it--)
	{
	}
	BiRingNode<int>* ptr=ring.GetNodeFromTail();
	ptr=ring.GetNodeFromTail();
}
void testKeyTree()
{
	KeyTree<string,char*> ktree("head");
	ktree.get_t_ref()="head";
	KeyTree<string,char*>::TreeNode* node=new KeyTree<string,char*>::TreeNode("I");
	KeyTree<string,char*>::TreeNode* node2=new KeyTree<string,char*>::TreeNode("am");
	KeyTree<string,char*>::TreeNode* node3=new KeyTree<string,char*>::TreeNode("cai");
	KeyTree<string,char*>::TreeNode* node4=new KeyTree<string,char*>::TreeNode("bin");
	node->t="I";
	node2->t="am";
	node3->t="cai";
	node4->t="bin";
	ktree.LinkToHead(node);
	ktree.LinkToHead(node2);
	node3->AddTo(node2);
	node4->AddTo(node2);
	printf("cur_first traverse:\n");
	for(KeyTree<string,char*>::iterator it=ktree.BeginIterate();it;it++)
	{
		printf("%s\n",it->t);
	}
	printf("cur_last traverse:\n");
	for(KeyTree<string,char*>::iterator it=ktree.BeginIterate(NULL,false);it;it++)
	{
		printf("%s\n",it->t);
	}
}
int MountDev(pintf_fsdrv pintf,char* base,void** dev)
{
	int ret=0;
	char mountcmd[100],formatcmd[100];
	sprintf_s(mountcmd,99,"mount native_fs base=%s",base);
	sprintf_s(formatcmd,99,"format native_fs base=%s",base);
	for(int i=0;i<2;i++)
	{
		if(0!=(ret=pintf->mount(mountcmd,dev)))
		{
			if(ret!=ERR_FS_DEV_MOUNT_FAILED_NOT_EXIST)
				return ret;
		}
		if(ret==0||i==1)
			return ret;
		if(0!=(ret=pintf->format(formatcmd)))
			return ret;
	}
	return ret;
}
#define safe_unmount(pintf,dev) \
	if(VALID(dev)) \
	{ \
		pintf->unmount(dev); \
		dev=NULL; \
	}
int testDllDrv()
{
	set_last_error(0);
	void* h=sys_load_library("fsdrv.dll");
	dword err=get_last_error();
	if(!VALID(h))
		return ERR_LOAD_LIBRARY_FAILED;
	void* addr=sys_get_lib_proc(h,"get_storage_drv_interface");
	pintf_fsdrv(*get_storage_drv_interface)(char*)=(pintf_fsdrv(*)(char*))addr;
	pintf_fsdrv pintf=get_storage_drv_interface("native_fs");
	void *dev=NULL,*dev2=NULL;
	int ret=0;
	vector<fsls_element> files;
	if(0!=(ret=pintf->init()))
		goto freelib;
	void** vdev[2]={&dev,&dev2};
	char* devname[2]={"hello_pan","goodbye_pan"};
	for(int i=0;i<2;i++)
	{
		if(0!=(ret=MountDev(pintf,devname[i],vdev[i])))
			goto unmount;
	}
	if(0!=(ret=pintf->mkdir(dev,"/test/abc")))
		goto unmount;
	void* hfile=pintf->open(dev,"/test/abc/1.txt",FILE_CREATE_ALWAYS|FILE_WRITE);
	if(!VALID(hfile))
		goto unmount;
	char* w="hello world";
	uint rdwr=0;
	ret=pintf->write(dev,hfile,0,0,strlen(w),w,&rdwr);
	ret=pintf->setsize(dev,hfile,15,0);
	ret=pintf->del(dev,"/test/");
	ret=pintf->mkdir(dev,"/disk1");
	ret=pintf->move(dev,"/test/abc/","/disk1");
	pintf->close(dev,hfile);
	ret=pintf->move(dev,"test/abc/","disk1/abc0");
	ret=pintf->listfiles(dev,"disk1/abc0",&files);
	hfile=pintf->open(dev,"/test/abc/1.txt",FILE_OPEN_EXISTING|FILE_READ);
	if(!VALID(hfile))
		goto unmount;
	uint size=0,szhigh=0;
	if(0!=(ret=pintf->getsize(dev,hfile,&size,&szhigh)))
		goto unmount;
	assert(szhigh==0);
	byte* buf=new byte[size];
	rdwr=0;
	ret=pintf->read(dev,hfile,0,0,size,buf,&rdwr);
	delete[] buf;
	pintf->close(dev,hfile);
	dword attr=0;
	ret=pintf->getattr(dev,"disk1",&attr);
	ret=pintf->setattr(dev,"disk1/abc0",attr);
	DateTime date[3];
	ret=pintf->getfiletime(dev,"disk1/abc0",FS_ATTR_DATE,date);
	ret=pintf->getfiletime(dev,"disk1/abc0/1.txt",FS_ATTR_DATE,date);
	ret=pintf->getattr(dev,"disk1/abc0/1.txt",&attr);
	ret=pintf->setfiletime(dev,"test/abc/1.txt",FS_ATTR_DATE,date);
unmount:
	safe_unmount(pintf,dev);
	safe_unmount(pintf,dev2);
	pintf->uninit();
freelib:
	sys_free_library(h);
	return ret;
}
struct test_gate_struct
{
	gate mutex;
	bool quit;
	test_gate_struct()
	{
		quit=false;
	}
};
struct per_thrd_st
{
	test_gate_struct* pt;
	int thrd_id;
	void* hthrd;
};
int cb_thrd_gate(void* param)
{
	per_thrd_st* st=(per_thrd_st*)param;
	printf("this is %dth thread.\n",st->thrd_id);
	while(!st->pt->quit)
	{
		st->pt->mutex.get_lock(true);
		printf("%dth get lock.\n",st->thrd_id);
		sys_sleep((rand()%10)+1);
		printf("%dth release lock.\n",st->thrd_id);
		st->pt->mutex.get_lock(false);
		sys_sleep((rand()%10)+1);
	}
	return 0;
}
int cb_thrd_msg_quit(void* param)
{
	sys_show_message("ok to quit?");
	*(bool*)param=true;
	return 0;
}
void test_gate()
{
	const uint nthrd=10;
	bool quit=false;
	void* hthrd;
	test_gate_struct ts;
	per_thrd_st st[nthrd];
	for(int i=0;i<nthrd;i++)
	{
		st[i].pt=&ts;
		st[i].thrd_id=i;
		st[i].hthrd=sys_create_thread(cb_thrd_gate,st+i);
	}
	hthrd=sys_create_thread(cb_thrd_msg_quit,&quit);
	while(!quit)
	{
		ts.mutex.cut_down(true);
		printf("turn off gate.\n");
		sys_sleep(10);
		printf("turn on gate.\n");
		ts.mutex.cut_down(false);
		sys_sleep(10);
	}
	sys_wait_thread(hthrd);
	sys_close_thread(hthrd);
	ts.quit=true;
	for(int i=0;i<nthrd;i++)
	{
		sys_wait_thread(st[i].hthrd);
		sys_close_thread(st[i].hthrd);
	}
}
class TPipe
{
public:
	TPipe(Pipe& pipe);
	void Feed(char* str);
	void Term();
private:
	Pipe* pp;
	void* hthrd;
	static int thrd_func(void* param);
};
TPipe::TPipe(Pipe& pipe)
{
	pp=&pipe;
	hthrd=sys_create_thread(thrd_func,pp);
	if(!VALID(hthrd))
		EXCEPTION(ERR_THREAD_CREATE_FAILED);
}
void TPipe::Feed(char* str)
{
	uint len;
	if((len=strlen(str))>0)
		pp->Send(str,len);
}
void TPipe::Term()
{
	pp->Send(NULL,0);
	sys_wait_thread(hthrd);
	sys_close_thread(hthrd);
	hthrd=NULL;
}
int TPipe::thrd_func(void* param)
{
	Pipe* pipe=(Pipe*)param;
	byte buf[6];
	uint recv=0;
	while((recv=pipe->Recv(buf,5))>0)
	{
		buf[recv]=0;
		printf("%s",buf);
	}
	return 0;
}
void testPipe()
{
	Pipe pipe;
	TPipe t(pipe);
	t.Feed("hello");
	t.Feed("world");
	t.Feed("caibin");
	t.Feed("isa");
	t.Feed("fool");
	t.Term();
}
void test_fwrite()
{
	char alpha[256];
	for(int i=0;i<256;i++)
	{
		alpha[i]=(char)i;
	}
	fwrite(alpha,256,1,stdout);
}
int _tmain(int argc, TCHAR** argv)
{
	int ret=0;
	if(0!=(ret=mainly_initial()))
		return ret;
	//printf("%d\n",ERR_GENERIC);
	//test_fs();
	//testfile();
	//test_read();
	//testtime();
	//test_int64(1);
	//test_int64(-1);
	//test_i64();
	//sys_create_process("notepad");
	//testBiRing();
	//testKeyTree();
	//testDllDrv();
	//test_gate();
	//testPipe();
	//test_fwrite();
	LOGFILE(0,log_ftype_info,"%s start OK",get_current_executable_name());
	process_stat pstat;
	init_process_stat(&pstat,"AstManager.exe");
	if_ids ifs;
	pstat.ifs=&ifs;
	get_executable_info(&pstat);
	mainly_exit();
	return 0;
}
