#include "datetime.h"
const uint weekday_epoch=5;//2000/1/1
const char* abbrev_weekday[]=
{
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat",
	"Sun",
};
const uint n_month_table[]=
{
	0,
	31,
	31+28,
	31+28+31,
	31+28+31+30,
	31+28+31+30+31,
	31+28+31+30+31+30,
	31+28+31+30+31+30+31,
	31+28+31+30+31+30+31+31,
	31+28+31+30+31+30+31+31+30,
	31+28+31+30+31+30+31+31+30+31,
	31+28+31+30+31+30+31+31+30+31+30,
	31+28+31+30+31+30+31+31+30+31+30+31,
};
const uint l_month_table[]=
{
	0,
	31,
	31+29,
	31+29+31,
	31+29+31+30,
	31+29+31+30+31,
	31+29+31+30+31+30,
	31+29+31+30+31+30+31,
	31+29+31+30+31+30+31+31,
	31+29+31+30+31+30+31+31+30,
	31+29+31+30+31+30+31+31+30+31,
	31+29+31+30+31+30+31+31+30+31+30,
	31+29+31+30+31+30+31+31+30+31+30+31,
};
CTimeSpan::CTimeSpan(uint _day,byte _hour,byte _minute,byte _second,ushort _millisecond)
{
	day=_day;
	hour=_hour;
	minute=_minute;
	second=_second;
	millisecond=_millisecond;
}
CTimeSpan::CTimeSpan(const TimeSpan& span)
{
	*(TimeSpan*)this=span;
}
bool CTimeSpan::Valid() const
{
	return hour<24&&minute<60&&second<60&&millisecond<1000;
}
bool CTimeSpan::operator<(const CTimeSpan& other) const
{
	if(day!=other.day)
		return day<other.day;
	else if(hour!=other.hour)
		return hour<other.hour;
	else if(minute!=other.minute)
		return minute<other.minute;
	else if(second!=other.second)
		return second<other.second;
	else
		return millisecond<other.millisecond;
}
bool CTimeSpan::operator>(const CTimeSpan& other) const
{
	if(day!=other.day)
		return day>other.day;
	else if(hour!=other.hour)
		return hour>other.hour;
	else if(minute!=other.minute)
		return minute>other.minute;
	else if(second!=other.second)
		return second>other.second;
	else
		return millisecond>other.millisecond;
}
bool CTimeSpan::operator<=(const CTimeSpan& other) const
{
	return !(*this>other);
}
bool CTimeSpan::operator>=(const CTimeSpan& other) const
{
	return !(*this<other);
}
bool CTimeSpan::operator==(const CTimeSpan& other) const
{
	return day==other.day
		&&hour==other.hour
		&&minute==other.minute
		&&second==other.second
		&&millisecond==other.millisecond;
}
bool CTimeSpan::operator!=(const CTimeSpan& other) const
{
	return !(*this==other);
}
bool CDateTime::work_day=false;
CDateTime operator+(const CDateTime& date, const TimeSpan& span)
{
	CDateTime rdate(date);
	rdate+=span;
	return rdate;
}
CDateTime operator-(const CDateTime& date, const TimeSpan& span)
{
	CDateTime rdate(date);
	rdate-=span;
	return rdate;
}
CDateTime::CDateTime(uint _year,byte _month,byte _day,
	byte _hour,byte _minute,byte _second,
	ushort _millisecond)
{
	year=_year;
	month=_month;
	day=_day;
	hour=_hour;
	minute=_minute;
	second=_second;
	millisecond=_millisecond;
	weekday=CalculateWeekDay();
}
CDateTime::CDateTime(const DateTime& date_time)
{
	*(DateTime*)this=date_time;
}
inline bool is_leap_year(int y)
{
	return (y%4==0&&y%100!=0)||y%400==0;
}
inline void __div__(int& q,int& r,int a,int b)
{
	q=(a>=0?a/b:((-a)%b==0?-((-a)/b):-((-a)/b+1)));
	r=a-q*b;
}
inline int clamp(int t, int a, int b)
{
	return t<a?a:(t>=b?b-1:t);
}
bool CDateTime::ValidDate() const
{
	if(month>=12)
		return false;
	const uint* m=is_leap_year((int)year)?l_month_table:n_month_table;
	if((uint)day>=m[(int)month+1]-m[(int)month])
		return false;
	if(hour>=24||minute>=60||second>=60||millisecond>=1000)
		return false;
	if(weekday!=CalculateWeekDay())
		return false;
	return true;
}
void CDateTime::InitWithCurrentDateTime()
{
	sys_get_date_time(this);
}
bool CDateTime::operator<(const CDateTime& other) const
{
	if(year!=other.year)
		return year<other.year;
	else if(month!=other.month)
		return month<other.month;
	else if(day!=other.day)
		return day<other.day;
	else if(hour!=other.hour)
		return hour<other.hour;
	else if(minute!=other.minute)
		return minute<other.minute;
	else if(second!=other.second)
		return second<other.second;
	else
		return millisecond<other.millisecond;
}
bool CDateTime::operator>(const CDateTime& other) const
{
	if(year!=other.year)
		return year>other.year;
	else if(month!=other.month)
		return month>other.month;
	else if(day!=other.day)
		return day>other.day;
	else if(hour!=other.hour)
		return hour>other.hour;
	else if(minute!=other.minute)
		return minute>other.minute;
	else if(second!=other.second)
		return second>other.second;
	else
		return millisecond>other.millisecond;
}
bool CDateTime::operator<=(const CDateTime& other) const
{
	return !(*this>other);
}
bool CDateTime::operator>=(const CDateTime& other) const
{
	return !(*this<other);
}
bool CDateTime::operator==(const CDateTime& other) const
{
	return year==other.year
		&&month==other.month
		&&day==other.day
		&&hour==other.hour
		&&minute==other.minute
		&&second==other.second
		&&millisecond==other.millisecond
		&&weekday==other.weekday;
}
bool CDateTime::operator!=(const CDateTime& other) const
{
	return !(*this==other);
}
CDateTime& CDateTime::operator+=(const TimeSpan& span)
{
	if(work_day&&weekday>=5)
		clear(FORMAT_TIME);
	int carry;
	int t=(int)millisecond+(int)span.millisecond;
	millisecond=(ushort)(t>=1000?(carry=1,t-1000):(carry=0,t));
	t=(int)second+(int)span.second+carry;
	second=(byte)(t>=60?(carry=1,t-60):(carry=0,t));
	t=(int)minute+(int)span.minute+carry;
	minute=(byte)(t>=60?(carry=1,t-60):(carry=0,t));
	t=(int)hour+(int)span.hour+carry;
	hour=(byte)(t>=24?(carry=1,t-24):(carry=0,t));
	int eday=ConvertToEpoch();
	if(work_day)
	{
		int q,r;
		__div__(q,r,(int)span.day+carry,5);
		eday+=q*7+((int)weekday>=5?(7-weekday+r):(r<5-(int)weekday?r:r+(7-5)));
	}
	else
	{
		eday+=((int)span.day+carry);
	}
	ConvertFromEpoch(eday);
	return *this;
}
CDateTime& CDateTime::operator-=(const TimeSpan& span)
{
	if(work_day&&weekday>=5)
		clear(FORMAT_TIME);
	int carry=0;
	int t=(int)span.millisecond+carry;
	millisecond=(ushort)((int)millisecond<t?(carry=1,(int)millisecond+1000-t):(carry=0,(int)millisecond-t));
	t=(int)span.second+carry;
	second=(byte)((int)second<t?(carry=1,(int)second+60-t):(carry=0,(int)second-t));
	t=(int)span.minute+carry;
	minute=(byte)((int)minute<t?(carry=1,(int)minute+60-t):(carry=0,(int)minute-t));
	t=(int)span.hour+carry;
	hour=(byte)((int)hour<t?(carry=1,(int)hour+24-t):(carry=0,(int)hour-t));
	int eday=ConvertToEpoch();
	if(work_day)
	{
		int q,r;
		__div__(q,r,-((int)span.day+carry),5);
		eday+=q*7+((int)weekday>=5?(7-(int)weekday+r):(r<5-(int)weekday?r:r+(7-5)));
	}
	else
	{
		eday-=((int)span.day+carry);
	}
	ConvertFromEpoch(eday);
	return *this;
}
TimeSpan CDateTime::operator-(const CDateTime& other) const
{
	CDateTime time1(*this),time2(other);
	if(work_day)
	{
		if(time1.weekday>=5)
			time1.clear(FORMAT_TIME);
		if(time2.weekday>=5)
			time2.clear(FORMAT_TIME);
	}
	if(time1>time2)
		swap(time1,time2);
	TimeSpan ts;
	int carry=0;
	int t=(int)time1.millisecond+carry;
	ts.millisecond=(ushort)((int)time2.millisecond<t?(carry=1,(int)time2.millisecond+1000-t):(carry=0,(int)time2.millisecond-t));
	t=(int)time1.second+carry;
	ts.second=(byte)((int)time2.second<t?(carry=1,(int)time2.second+60-t):(carry=0,(int)time2.second-t));
	t=(int)time1.minute+carry;
	ts.minute=(byte)((int)time2.minute<t?(carry=1,(int)time2.minute+60-t):(carry=0,(int)time2.minute-t));
	t=(int)time1.hour+carry;
	ts.hour=(byte)((int)time2.hour<t?(carry = 1,(int)time2.hour+24-t):(carry=0,(int)time2.hour-t));
	int eday1=time1.ConvertToEpoch(),
		eday2=time2.ConvertToEpoch();
	if(work_day)
	{
		int q,r;
		if(carry>0)
		{
			if((int)time2.weekday==0)
				carry+=2;
			else if((int)time2.weekday>=5)
				carry+=((int)time2.weekday-5);
		}
		__div__(q,r,eday2-eday1-carry,7);
		ts.day=(uint)(q*5+((int)time1.weekday>=5?clamp(r-(7-(int)time1.weekday),0,5):(r<5-(int)time1.weekday?r:clamp(r-(7-5),5-(int)time1.weekday,5))));
	}
	else
	{
		ts.day=(uint)(eday2-eday1-carry);
	}
	return ts;
}
void CDateTime::clear(dword flags)
{
	if(flags&FORMAT_DATE)
	{
		year=2000;
		month=0;
		day=0;
		weekday=(byte)weekday_epoch;
	}
	if(flags&FORMAT_TIME)
	{
		hour=0;
		minute=0;
		second=0;
		millisecond=0;
	}
}
int CDateTime::ConvertToEpoch() const
{
	int offyear=(int)year-2000;
	int q,r;
	__div__(q,r,offyear,400);
	int baseday=q*(400*365+100-4+1);
	int remyear=r;
	__div__(q,r,remyear,100);
	bool epoch=(q==0);
	baseday+=q*(100*365+25-1)+(q==0?0:1);
	remyear=r;
	__div__(q,r,remyear,4);
	baseday+=q*(4*365+1)-((epoch||q==0)?0:1);
	baseday+=r*365+(((!epoch&&q==0)||r==0)?0:1);
	baseday+=((int)(is_leap_year((int)year)?
		l_month_table:n_month_table)[(int)month]
		+(int)day);
	return baseday;
}
void CDateTime::ConvertFromEpoch(int days)
{
	int q,r;
	__div__(q,r,days,400*365+100-4+1);
	int baseyear=2000+q*400;
	int remday=r;
	bool epoch=true;
	if(remday>=100*365+25)
	{
		__div__(q,r,remday-1,100*365+25-1);
		epoch=false;
		baseyear+=(q*100);
		remday=r;
	}
	bool leap=epoch;
	if(remday>=4*365+(!epoch?0:1))
	{
		leap = true;
		__div__(q,r,remday+(epoch?0:1),4*365+1);
		baseyear+=(q*4);
		remday=r;
	}
	if(remday>=365+(!leap?0:1))
	{
		__div__(q,r,remday-(!leap?0:1),365);
		baseyear+=q;
		remday=r;
	}
	year=(short)baseyear;
	const uint* m=is_leap_year((int)year)?l_month_table:n_month_table;
	for(int i=0;i<12;i++)
	{
		if(remday<(int)m[i+1])
		{
			month=(byte)i;
			day=(byte)(remday-(int)m[i]);
			break;
		}
	}
	int d=(int)weekday_epoch+days;
	__div__(q,r,d,7);
	weekday=(byte)r;
}
byte CDateTime::CalculateWeekDay() const
{
	int d=ConvertToEpoch();
	int q,r;
	d+=(int)weekday_epoch;
	__div__(q,r,d,7);
	return (byte)r;
}
void CDateTime::Format(string& str,dword flags,char* sepday,char* septime,char* sep) const
{
	char strbuf[256];
	str.clear();
	if(flags&FORMAT_DATE)
	{
		sprintf(strbuf,"%d%s%02d%s%02d",(int)year,sepday,(int)month+1,sepday,(int)day+1);
		str=strbuf;
		if(flags&FORMAT_WEEKDAY)
		{
			sprintf(strbuf,"%s",abbrev_weekday[weekday]);
			str+=string(" ")+strbuf;
		}
	}
	if(flags&FORMAT_TIME)
	{
		sprintf(strbuf,"%02d%s%02d%s%02d",(int)hour,septime,(int)minute,septime,(int)second);
		str+=string((flags&FORMAT_DATE)?sep:"")+string(strbuf);
		if(flags&FORMAT_MILLISEC)
		{
			sprintf(strbuf,".%03d",(int)millisecond);
			str+=strbuf;
		}
	}
}
void CDateTime::SetWorkDay(bool workday)
{
	work_day=workday;
}
bool CDateTime::GetWorkDay()
{
	return work_day;
}
