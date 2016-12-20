// Code copied from: http://read.pudn.com/downloads75/sourcecode/multimedia/277333/SunRiseSet.H__.htm
// Adjusted to use POCO, L. Meyer, 2015-06-12

#ifndef _SUNRISESET_H 
#define _SUNRISESET_H 
#include <string>
#include "math.h" 
 
#define PI 3.1415926 
 
class CMonth 
{ 
public: 
   CMonth() {} 
   CMonth(std::string csName,int iNumDays) { m_iNumDays = iNumDays; m_csName = csName;} 
 
	int m_iNumDays; 
	std::string m_csName; 
 
};	 
 
static CMonth monthList[] = 
{ 
	CMonth("January",31), 
	CMonth("February",28), 
	CMonth("March",31), 
	CMonth("April",30), 
	CMonth("May",31), 
	CMonth("June",30), 
	CMonth("July",31), 
	CMonth("August",31), 
	CMonth("September",30), 
	CMonth("October",31), 
	CMonth("November",30), 
	CMonth("December",31) 
};  
static CMonth leapList[] = 
{ 
	CMonth("January",31), 
	CMonth("February",29), 
	CMonth("March",31), 
	CMonth("April",30), 
	CMonth("May",31), 
	CMonth("June",30), 
	CMonth("July",31), 
	CMonth("August",31), 
	CMonth("September",30), 
	CMonth("October",31), 
	CMonth("November",30), 
	CMonth("December",31) 
};  
 
 
class CSunRiseSet 
{ 
public: 
	CSunRiseSet() {} 
 
	//Returns in GMT 
	Poco::DateTime GetSunset(double dLat,double dLon,Poco::DateTime time); 
	Poco::DateTime GetSunrise(double dLat,double dLon,Poco::DateTime time); 
	Poco::DateTime GetSolarNoon(double dLon, Poco::DateTime time); 
private: 
	//	Convert radian angle to degrees 
	double dRadToDeg(double dAngleRad) 
	{ 
		return (180 * dAngleRad / PI); 
	} 
 
	//	Convert degree angle to radians 
	double dDegToRad(double dAngleDeg) 
	{ 
		return (PI * dAngleDeg / 180); 
	} 
 
	double CalcGamma(int iJulianDay) 
	{ 
		return (2 * PI / 365) * (iJulianDay - 1);	 
	} 
 
 
	double CalcGamma2(int iJulianDay, int iHour) 
	{ 
		return (2 * PI / 365) * (iJulianDay - 1 + (iHour/24)); 
	} 
 
	double CalcEqofTime(double dGamma) 
 
	{ 
		return (229.18 * (0.000075 + 0.001868 * cos(dGamma) - 0.032077 * sin(dGamma)- 0.014615 * cos(2 * dGamma) - 0.040849 *  sin(2 * dGamma))); 
	} 
	 
	double CalcSolarDec(double dGamma) 
	{ 
		return (0.006918 - 0.399912 * cos(dGamma) + 0.070257 * sin(dGamma) - 0.006758 * cos(2 * dGamma) + 0.000907 *  sin(2 * dGamma)); 
	} 
 
	int CalcDayLength(double dHourAngle) 
	{ 
	//	Return the length of the day in minutes. 
		return (int)((2 * abs(dRadToDeg(dHourAngle))) / 15); 
 
	} 
 
 
	std::string JulianDayToDate(int jday, bool bLeapYear); 
	double CalcJulianDay(int iMonth,int iDay, bool bLeapYr); 
 
	double CalcHourAngle(double dLat, double dSolarDec, bool bTime); 
	 
	double calcSunsetGMT(int iJulDay, double dLatitude, double dLongitude); 
	double calcSunriseGMT(int iJulDay, double dLatitude, double dLongitude); 
	double calcSolNoonGMT(int iJulDay, double dLongitude); 
 
	double findRecentSunrise(int iJulDay, double dLatitude, double dLongitude); 
	double findRecentSunset(int iJulDay, double dLatitude, double dLongitude); 
 
	double findNextSunrise(int iJulDay, double dLatitude, double dLongitude); 
	double findNextSunset(int iJulDay, double dLatitude, double dLongitude); 
 
	bool IsInteger(double dValue); 
	bool IsLeapYear(int iYear) 
	{ 
		return ((iYear % 4 == 0 && iYear % 100 != 0) || iYear % 400 == 0);	 
	} 
 
}; 
 
 
 
#endif
