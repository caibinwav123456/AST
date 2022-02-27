#ifdef USE_FS_ENV_VAR
#include "fsenv.h"
#include "utility.h"
#include "string.h"
static const byte available_token_char[]={'_'};
static const spec_char_verifier token_vrf(available_token_char,sizeof(available_token_char)/sizeof(byte),true);
static inline bool is_valid_env_name(const char* envname)
{
	if(*envname==0||(*envname>='0'&&*envname<='9'))
		return false;
	for(const char* pstr=envname;*pstr!=0;pstr++)
	{
		if(!token_vrf.is_spec(*pstr))
			return false;
	}
	return true;
}
int FSEnvSet::SetEnv(const string& envname,const string& envval)
{
	if(!is_valid_env_name(envname.c_str()))
		return ERR_INVALID_ENV_NAME;
	if(envval.empty())
	{
		map<string,string>::iterator it=env_assoc.find(envname);
		if(it!=env_assoc.end())
			env_assoc.erase(it);
		return 0;
	}
	env_assoc[envname]=envval;
	return 0;
}
int FSEnvSet::FindEnv(const string& envname,string& envval) const
{
	if(!is_valid_env_name(envname.c_str()))
		return ERR_INVALID_ENV_NAME;
	map<string,string>::const_iterator it=env_assoc.find(envname);
	if(it==env_assoc.end())
		return ERR_ENV_NOT_FOUND;
	envval=it->second;
	return 0;
}
int FSEnvSet::ReplaceEnv(const string& origin,string& output) const
{
	return 0;
}
#endif
