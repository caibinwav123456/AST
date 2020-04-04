#include "syslog.h"
#include "config_val_extern.h"
#include "utility.h"
#include "path.h"
#include "datetime.h"
DEFINE_STRING_VAL(sys_log_path,"");
DEFINE_STRING_VAL(sys_log_sem,"sem_syslog");
LogSys::LogSys()
{
	inited=false;
	sem=NULL;
}
int LogSys::Init()
{
	int ret=0;
	if(0!=(ret=get_absolute_path(string(get_current_executable_path()),sys_log_path,full_sys_log_path)))
		return ret;
	sem=(is_launcher()?sys_create_sem(1,1,(char*)sys_log_sem.c_str()):sys_get_sem((char*)sys_log_sem.c_str()));
	if(!VALID(sem))
		return ERR_SEM_CREATE_FAILED;
	inited=true;
	return ret;
}
void LogSys::Exit()
{
	if(VALID(sem))
	{
		sys_close_sem(sem);
		sem=NULL;
	}
	inited=false;
}
int LogSys::Log(const char*const* lognames,const char* file,int line,const string& logmsg)
{
	char buf[1025];
	string str;
	if(!inited)
		return 0;
	sys_mkdir((char*)full_sys_log_path.c_str());
	if(!VALID(sem))
		return ERR_SEM_CREATE_FAILED;
	int ret=0;
	CDateTime ctime;
	ctime.InitWithCurrentDateTime();
	ctime.Format(str,FORMAT_DATE|FORMAT_TIME);
	sprintf(buf,"[%s][%s] file:%s line:%d %s\n",str.c_str(),get_current_executable_name(),file,line,logmsg.c_str());
	string msg(buf);
	ctime.Format(str,FORMAT_DATE,"-");
	for(const char*const* p=lognames;*p!=NULL;p++)
	{
		sprintf(buf,"log_%s_%s.log",*p,str.c_str());
		string fulllogpath;
		concat_path(full_sys_log_path,string(buf),fulllogpath);
		sys_wait_sem(sem);
		void* hfile=sys_fopen((char*)fulllogpath.c_str(),FILE_OPEN_ALWAYS|FILE_WRITE);
		if(!VALID(hfile))
		{
			ret=ERR_FILE_IO;
			goto final;
		}
		if(0!=(ret=sys_fseek(hfile,0,NULL,SEEK_END)))
			goto final1;
		if(0!=(ret=sys_fwrite(hfile,(char*)msg.c_str(),msg.size())))
			goto final1;
final1:
		sys_fclose(hfile);
final:
		sys_signal_sem(sem);
	}
	return ret;
}
