// Reference: http://www.codeproject.com/Articles/645/Sunrise-Sunset-Calculations
// Adjusted to use POCO, L. Meyer, 2015-06-12

#include "math.h" 

#include "Poco/DateTime.h"
#include "Poco/Timezone.h"
#include "Poco/NumberFormatter.h"

#include "SunRiseSet.h" 
 
std::string CSunRiseSet::JulianDayToDate(int jday, bool bLeapYear) 
{ 
	std::string strDate; 
	int day = jday; 
	int mon = 0; 
 
	if(bLeapYear)  
	{ 
		for (int i = 0; i < 12; i ++)  
		{ 
			if (day > leapList[i].m_iNumDays) 
			{ 
				mon ++; 
				day -= leapList[i].m_iNumDays; 
			} 
		} 
	} 
	else 
	{ 
		for (int i = 0; i < 12; i ++)  
		{ 
			if (day > monthList[i].m_iNumDays) 
			{ 
				mon ++; 
				day -= monthList[i].m_iNumDays; 
			} 
		} 
	} 
		 
		return monthList[mon].m_csName + Poco::NumberFormatter::format(day);
} 
 
double CSunRiseSet::CalcJulianDay(int iMonth,int iDay, bool bLeapYr) 
{ 
	int iJulDay = 0; 
 
	if(bLeapYr) 
	{ 
		for (int i = 0; i < iMonth - 1 ; i++) 
			iJulDay += leapList[i].m_iNumDays; 
	} 
	else 
	{ 
		for (int i = 0; i < iMonth - 1; i++) 
			iJulDay += monthList[i].m_iNumDays; 
	 
	} 
 
	iJulDay += iDay; 
 
	return iJulDay; 
} 
 
//	The hour angle returned below is only for sunrise/sunset, i.e. when the solar zenith angle is 90.8 degrees. 
// the reason why it's not 90 degrees is because we need to account for atmoshperic refraction. 
 
double CSunRiseSet::CalcHourAngle(double dLat, double dSolarDec, bool bTime) 
{ 
		 
/* 
		var latRad = degToRad(lat); 
		if (time) 		//	ii true, then calculationg for sunrise 
 
			return (Math.acos(Math.cos(degToRad(90.833))/(Math.cos(latRad)*Math.cos(solarDec))-Math.tan(latRad) * Math.tan(solarDec))); 
 
		else 
 
			return -(Math.acos(Math.cos(degToRad(90.833))/(Math.cos(latRad)*Math.cos(solarDec))-Math.tan(latRad) * Math.tan(solarDec)));		 
*/ 
	 
	 
	double dLatRad = dDegToRad(dLat); 
	if(bTime)  //Sunrise 
	{ 
/*		double dTemp = cos(dDegToRad(90.833)); 
//		dTemp /= (cos(dLatRad)*cos(dSolarDec)); 
		dTemp /= ((cos(dLatRad)*cos(dSolarDec)) - (tan(dLatRad) * tan(dSolarDec))); 
//		dTemp -= (tan(dLatRad) * tan(dSolarDec)); 
		dTemp = acos(dTemp); 
		return dTemp; 
*/		 
		return  (acos(cos(dDegToRad(90.833))/(cos(dLatRad)*cos(dSolarDec))-tan(dLatRad) * tan(dSolarDec))); 
	} 
	else 
	{ 
/*		double dTemp = cos(dDegToRad(90.833)); 
		dTemp /= ((cos(dLatRad)*cos(dSolarDec)) - (tan(dLatRad) * tan(dSolarDec))); 
		//dTemp -= (tan(dLatRad) * tan(dSolarDec)); 
		dTemp = acos(dTemp); 
		return -dTemp; 
*/ 
		return -(acos(cos(dDegToRad(90.833))/(cos(dLatRad)*cos(dSolarDec))-tan(dLatRad) * tan(dSolarDec)));		 
	}	 
 
} 
 
double CSunRiseSet::calcSunsetGMT(int iJulDay, double dLatitude, double dLongitude) 
{ 
		// First calculates sunrise and approx length of day 
 
		double dGamma = CalcGamma(iJulDay + 1); 
		double eqTime = CalcEqofTime(dGamma); 
		double solarDec = CalcSolarDec(dGamma); 
		double hourAngle = CalcHourAngle(dLatitude, solarDec, 0); 
		double delta = dLongitude - dRadToDeg(hourAngle); 
		double timeDiff = 4 * delta; 
		double setTimeGMT = 720 + timeDiff - eqTime; 
 
		// first pass used to include fractional day in gamma calc 
 
		double gamma_sunset = CalcGamma2(iJulDay, setTimeGMT/60); 
		eqTime = CalcEqofTime(gamma_sunset); 
 
 
		solarDec = CalcSolarDec(gamma_sunset); 
		 
		hourAngle = CalcHourAngle(dLatitude, solarDec, false); 
		delta = dLongitude - dRadToDeg(hourAngle); 
		timeDiff = 4 * delta; 
		setTimeGMT = 720 + timeDiff - eqTime; // in minutes 
 
		return setTimeGMT; 
	} 
 
 
double CSunRiseSet::calcSunriseGMT(int iJulDay, double dLatitude, double dLongitude) 
{ 
	// *** First pass to approximate sunrise 
 
	double gamma = CalcGamma(iJulDay); 
	double eqTime = CalcEqofTime(gamma); 
	double solarDec = CalcSolarDec(gamma); 
	double hourAngle = CalcHourAngle(dLatitude, solarDec, true); 
	double delta = dLongitude - dRadToDeg(hourAngle); 
	double timeDiff = 4 * delta; 
	double timeGMT = 720 + timeDiff - eqTime; 
 
	// *** Second pass includes fractional jday in gamma calc 
 
	double gamma_sunrise = CalcGamma2(iJulDay, timeGMT/60); 
	eqTime = CalcEqofTime(gamma_sunrise); 
	solarDec = CalcSolarDec(gamma_sunrise); 
	hourAngle = CalcHourAngle(dLatitude, solarDec, 1); 
	delta = dLongitude - dRadToDeg(hourAngle); 
	timeDiff = 4 * delta; 
	timeGMT = 720 + timeDiff - eqTime; // in minutes 
 
	return timeGMT; 
} 
 
double  CSunRiseSet::calcSolNoonGMT(int iJulDay, double dLongitude) 
{ 
	// Adds approximate fractional day to julday before calc gamma 
 
	double gamma_solnoon = CalcGamma2(iJulDay, 12 + (dLongitude/15)); 
	double eqTime = CalcEqofTime(gamma_solnoon); 
//	double solarNoonDec = CalcSolarDec(gamma_solnoon); 
	double solNoonGMT = 720 + (dLongitude * 4) - eqTime; // min 
	 
	return solNoonGMT; 
} 
 
Poco::DateTime CSunRiseSet::GetSunrise(double dLat,double dLon, Poco::DateTime time) 
{ 
	bool bLeap = IsLeapYear(time.year()); 
	 
	int iJulianDay = CalcJulianDay(time.month(),time.day(),bLeap); 
 
	double timeGMT = calcSunriseGMT(iJulianDay, dLat,dLon);  
 
 
	// if Northern hemisphere and spring or summer, use last sunrise and next sunset 
	if ((dLat > 66.4) && (iJulianDay > 79) && (iJulianDay < 267)) 
		timeGMT = findRecentSunrise(iJulianDay, dLat, dLon); 
	// if Northern hemisphere and fall or winter, use next sunrise and last sunset 
	else if ((dLat > 66.4) && ((iJulianDay < 83) || (iJulianDay > 263))) 
		timeGMT = findNextSunrise(iJulianDay, dLat, dLon); 
	// if Southern hemisphere and fall or winter, use last sunrise and next sunset 
	else if((dLat < -66.4) && ((iJulianDay < 83) || (iJulianDay > 263))) 
		timeGMT = findRecentSunrise(iJulianDay, dLat, dLon); 
	// if Southern hemisphere and spring or summer, use next sunrise and last sunset 
	else if((dLat < -66.4) && (iJulianDay > 79) && (iJulianDay < 267)) 
		timeGMT = findNextSunrise(iJulianDay, dLat, dLon); 
//	else  
//	{ 
		//("Unaccountable Missing Sunrise!"); 
//	} 
	 
	 
	double dHour = timeGMT / 60; 
	int iHour = (int)dHour; 
	double dMinute = 60 * (dHour - iHour); 
	int iMinute = (int)dMinute; 
	double dSecond = 60 * (dMinute - iMinute); 
	int iSecond = (int)dSecond; 
	 
	Poco::DateTime NewTime(time.year(),time.month(),time.day(),iHour,iMinute,iSecond); 
	NewTime.makeLocal(Poco::Timezone::tzd() - Poco::Timezone::dst()); 
	return NewTime;
} 
 
Poco::DateTime CSunRiseSet::GetSunset(double dLat,double dLon,Poco::DateTime time) 
{ 
	bool bLeap = IsLeapYear(time.year()); 
	 
	int iJulianDay = CalcJulianDay(time.month(),time.day(),bLeap); 
 
	double timeGMT = calcSunsetGMT(iJulianDay, dLat,dLon);  
 
	// if Northern hemisphere and spring or summer, use last sunrise and next sunset 
	if ((dLat > 66.4) && (iJulianDay > 79) && (iJulianDay < 267)) 
		timeGMT = findRecentSunset(iJulianDay, dLat, dLon); 
	// if Northern hemisphere and fall or winter, use next sunrise and last sunset 
	else if ((dLat > 66.4) && ((iJulianDay < 83) || (iJulianDay > 263))) 
		timeGMT = findNextSunset(iJulianDay, dLat, dLon); 
	// if Southern hemisphere and fall or winter, use last sunrise and next sunset 
	else if((dLat < -66.4) && ((iJulianDay < 83) || (iJulianDay > 263))) 
		timeGMT = findRecentSunset(iJulianDay, dLat, dLon); 
	// if Southern hemisphere and spring or summer, use next sunrise and last sunset 
	else if((dLat < -66.4) && (iJulianDay > 79) && (iJulianDay < 267)) 
		timeGMT = findNextSunset(iJulianDay, dLat, dLon); 
//	else  
//	{ 
		//("Unaccountable Missing Sunrise!"); 
//	} 
		 
	double dHour = timeGMT / 60; 
	int iHour = (int)dHour; 
	double dMinute = 60 * (dHour - iHour); 
	int iMinute = (int)dMinute; 
	double dSecond = 60 * (dMinute - iMinute); 
	int iSecond = (int)dSecond; 
	 
	Poco::DateTime NewTime(time.year(),time.month(),time.day(),iHour,iMinute,iSecond); 
	NewTime.makeLocal(Poco::Timezone::tzd() - Poco::Timezone::dst()); 
	return NewTime; 
} 
 
Poco::DateTime CSunRiseSet::GetSolarNoon(double dLon, Poco::DateTime time) 
{ 
	bool bLeap = IsLeapYear(time.year()); 
	 
	int iJulianDay = CalcJulianDay(time.month(),time.day(),bLeap); 
 
	double timeGMT = calcSolNoonGMT(iJulianDay, dLon);  
 
	double dHour = timeGMT / 60; 
	int iHour = (int)dHour; 
	double dMinute = 60 * (dHour - iHour); 
	int iMinute = (int)dMinute; 
	double dSecond = 60 * (dMinute - iMinute); 
	int iSecond = (int)dSecond; 
	 
	Poco::DateTime NewTime(time.year(),time.month(),time.day(),iHour,iMinute,iSecond); 
	NewTime.makeLocal(Poco::Timezone::tzd() - Poco::Timezone::dst()); 
	return NewTime;
} 
 
double CSunRiseSet::findRecentSunrise(int iJulDay, double dLatitude, double dLongitude) 
{ 
	int jday = iJulDay; 
 
	double dTime = calcSunriseGMT(jday, dLatitude, dLongitude); 
	 
	while(!IsInteger(dTime) ) 
	{ 
		jday--; 
		if (jday < 1)  
			jday = 365; 
		dTime = calcSunriseGMT(jday, dLatitude, dLongitude); 
	} 
 
	return jday; 
} 
	 
bool CSunRiseSet::IsInteger(double dValue) 
{ 
	int iTemp = (int)dValue; 
	double dTemp = dValue - iTemp; 
	if(dTemp == 0) 
		return true; 
	else 
		return false; 
} 
 
double CSunRiseSet::findRecentSunset(int iJulDay, double dLatitude, double dLongitude) 
{ 
	int jday = iJulDay; 
 
	double dTime = calcSunsetGMT(jday, dLatitude, dLongitude); 
	 
	while(!IsInteger(dTime) ) 
	{ 
		jday--; 
		if (jday < 1)  
			jday = 365; 
		dTime = calcSunsetGMT(jday, dLatitude, dLongitude); 
	} 
 
	return jday; 
} 
 
 
double CSunRiseSet::findNextSunrise(int iJulDay, double dLatitude, double dLongitude) 
{ 
	int jday = iJulDay; 
 
	double dTime = calcSunriseGMT(jday, dLatitude, dLongitude); 
	 
	while(!IsInteger(dTime) ) 
	{ 
        jday++; 
		if (jday > 366)  
			jday = 1; 
        dTime = calcSunriseGMT(jday, dLatitude, dLongitude); 
	} 
 
    return jday; 
} 
 
double CSunRiseSet::findNextSunset(int iJulDay, double dLatitude, double dLongitude) 
{ 
	int jday = iJulDay; 
 
	double dTime = calcSunsetGMT(jday, dLatitude, dLongitude); 
	 
	while(!IsInteger(dTime) ) 
	{ 
        jday++; 
		if (jday > 366)  
			jday = 1; 
        dTime = calcSunsetGMT(jday, dLatitude, dLongitude); 
	} 
 
    return jday; 
} 
