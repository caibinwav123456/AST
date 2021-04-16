#ifndef _DATE_TIME_H_
#define _DATE_TIME_H_
#include "common.h"
#include <string>
using namespace std;
#define FORMAT_DATE     1
#define FORMAT_TIME     2
#define FORMAT_WEEKDAY  4
#define FORMAT_MILLISEC 8
struct TimeSpan
{
	uint day;
	byte hour;
	byte minute;
	byte second;
	ushort millisecond;
};
class DLL CTimeSpan : public TimeSpan
{
public:
	CTimeSpan(uint _day=0,byte _hour=0,
		byte _minute=0,byte _second=0,ushort _millisecond=0);
	CTimeSpan(const TimeSpan& span);
	bool Valid() const;
	bool operator<(const CTimeSpan& other) const;
	bool operator>(const CTimeSpan& other) const;
	bool operator<=(const CTimeSpan& other) const;
	bool operator>=(const CTimeSpan& other) const;
	bool operator==(const CTimeSpan& other) const;
	bool operator!=(const CTimeSpan& other) const;
};
class DLL CDateTime : public DateTime
{
	friend DLL CDateTime operator+(const CDateTime& date,const TimeSpan& span);
	friend DLL CDateTime operator-(const CDateTime& date,const TimeSpan& span);
public:
	CDateTime(uint _year=2000,byte _month=0,byte _day=0,
		byte _hour=0,byte _minute=0,byte _second=0,
		ushort _millisecond=0);
	CDateTime(const DateTime& date_time);
	bool ValidDate() const;
	void InitWithCurrentDateTime();
	bool operator<(const CDateTime& other) const;
	bool operator>(const CDateTime& other) const;
	bool operator<=(const CDateTime& other) const;
	bool operator>=(const CDateTime& other) const;
	bool operator==(const CDateTime& other) const;
	bool operator!=(const CDateTime& other) const;
	CDateTime& operator+=(const TimeSpan& span);
	CDateTime& operator-=(const TimeSpan& span);
	TimeSpan operator-(const CDateTime& other) const;
	void clear(dword flags);
	int ConvertToEpoch() const;//2000/1/1
	void ConvertFromEpoch(int days);
	byte CalculateWeekDay() const;
	void Format(string& str,dword flags,char* sepday="/",char* septime=":",char* sep=" ") const;
	static void SetWorkDay(bool workday);
	static bool GetWorkDay();
private:
	static bool work_day;
};
#endif