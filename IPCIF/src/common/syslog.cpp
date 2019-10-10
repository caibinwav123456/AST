#include "syslog.h"
#include "config_val_extern.h"
#include "utility.h"
#include "path.h"
#include "datetime.h"
DEFINE_STRING_VAL(sys_log_path,"");
DEFINE_STRING_VAL(sys_log_sem,"sem_syslog");
LogSys::LogSys()
{
	sem=NULL;
}
int LogSys::Init()
{
	int ret=0;
	if(0!=(ret=get_absolute_path(string(get_current_executable_path()),sys_log_path,full_sys_log_path)))
		return ret;
	sem=(is_launcher()?sys_create_sem(1,1,(char*)sys_log_sem.c_str()):sys_get_sem((char*)sys_log_sem.c_str()));
	if(!VALID(sem))
		return ERR_GENERIC;
	return ret;
}
void LogSys::Exit()
{
	if(VALID(sem))
	{
		sys_close_sem(sem);
		sem=NULL;
	}
}
int LogSys::Log(const char*const* lognames,const char* file,int line,const string& logmsg)
{
	char buf[1025];
	string str;
	sys_mkdir((char*)full_sys_log_path.c_str());
	if(!VALID(sem))
		return ERR_GENERIC;
	int ret=0;
	CDateTime ctime;
	ctime.InitWithCurrentDateTime();
	for(const char*const* p=lognames;*p!=NULL;p++)
	{
		ctime.Format(str,FORMAT_DATE,"-");
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
		ctime.Format(str,FORMAT_DATE|FORMAT_TIME);
		sprintf(buf,"[%s][%s] file:%s line:%d %s\n",str.c_str(),get_current_executable_name(),file,line,logmsg.c_str());
		if(0!=(ret=sys_fseek(hfile,0,NULL,SEEK_END)))
			goto final1;
		if(0!=(ret=sys_fwrite(hfile,buf,strlen(buf))))
			goto final1;
final1:
		sys_fclose(hfile);
final:
		sys_signal_sem(sem);
	}
	return ret;
}
