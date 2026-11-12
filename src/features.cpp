#include "model.hpp"
#include <cwchar>
#include <limits>

namespace dw {
const std::array<Theme, ThemeCount> Themes{{
    {L"Minimal",L"Segoe UI Light",0xF2F4F1,0x91B89D},
    {L"Compact",L"Inter",0xF2F4F1,0x91B89D},
    {L"Editorial",L"DM Sans Semibold",0xFFF5E5,0xD9927B},
    {L"Stacked",L"Manrope Light",0xFFFFFF,0xA9D9EE},
    {L"Soft",L"Inter Light",0xFFF6EA,0xC6B5EA}
}};
void applyTheme(Config& c,int theme) {
    c.preset=std::clamp(theme,0,ThemeCount-1);const auto& t=Themes[c.preset];
    for(auto& w:c.widgets){w.font=t.font;w.color=t.text;w.accent=t.accent;w.customColors=false;}
}
long long systemSeconds(const SYSTEMTIME& t) {
    FILETIME ft{};if(!SystemTimeToFileTime(&t,&ft))return 0;
    ULARGE_INTEGER v{};v.LowPart=ft.dwLowDateTime;v.HighPart=ft.dwHighDateTime;
    return static_cast<long long>(v.QuadPart/10000000);
}
SYSTEMTIME secondsSystem(long long s) {
    ULARGE_INTEGER v{};v.QuadPart=static_cast<ULONGLONG>(std::max(0LL,s))*10000000;
    FILETIME ft{v.LowPart,v.HighPart};SYSTEMTIME t{};FileTimeToSystemTime(&ft,&t);return t;
}
long long utcNow(){SYSTEMTIME t{};GetSystemTime(&t);return systemSeconds(t);}
long long focusRemaining(const FocusState& f,long long now){return std::max(0LL,f.running?f.deadline-now:f.remaining);}
bool finishFocus(FocusState& f,long long now) {
    if(!f.running||f.deadline>now)return false;
    f.running=false;f.remaining=0;f.pending=true;return true;
}
void focusAction(FocusState& f,int action,long long now) {
    finishFocus(f,now);
    if(action==2)f.isBreak=!f.isBreak;
    if(action==1||action==2){f.running=false;f.remaining=(f.isBreak?f.breakMinutes:f.focusMinutes)*60;f.deadline=0;f.pending=false;}
    else if(f.running){f.remaining=focusRemaining(f,now);f.running=false;f.deadline=0;}
    else {if(f.remaining<=0)f.remaining=(f.isBreak?f.breakMinutes:f.focusMinutes)*60;f.deadline=now+f.remaining;f.running=true;f.pending=false;}
}
std::wstring countdownText(long long target,long long now) {
    if(!target)return L"Set a date";
    auto delta=target-now;if(delta<=0)return L"Reached";
    auto minutes=(delta+59)/60;
    if(delta<86400)return L"Today · "+std::to_wstring(minutes/60)+L"h "+std::to_wstring(minutes%60)+L"m";
    return std::to_wstring(delta/86400)+L"d "+std::to_wstring(delta/3600%24)+L"h "+std::to_wstring(delta/60%60)+L"m";
}
std::wstring batteryText(const SYSTEM_POWER_STATUS& p,bool available) {
    if(!available)return L"Unavailable";
    if(p.BatteryFlag!=255&&(p.BatteryFlag&128))return L"AC power";
    if(p.BatteryLifePercent==255)return L"Unavailable";
    return std::to_wstring(p.BatteryLifePercent)+L"%";
}
std::vector<DYNAMIC_TIME_ZONE_INFORMATION> timeZones() {
    std::vector<DYNAMIC_TIME_ZONE_INFORMATION> zones;
    DYNAMIC_TIME_ZONE_INFORMATION z{};
    for(DWORD i=0;EnumDynamicTimeZoneInformation(i,&z)==ERROR_SUCCESS;++i)zones.push_back(z);
    return zones;
}
bool zoneTime(const std::wstring& key,const SYSTEMTIME& utc,SYSTEMTIME& local) {
    if(key==L"UTC"){local=utc;return true;}
    static const auto zones=timeZones();
    for(const auto& z:zones)if(key==z.TimeZoneKeyName)return SystemTimeToTzSpecificLocalTimeEx(&z,&utc,&local)!=FALSE;
    local=utc;return false;
}
bool localTarget(const std::wstring& value,long long& utc) {
    int y{},m{},d{},h{},n{};wchar_t tail{};
    if(swscanf(value.c_str(),L"%d-%d-%d %d:%d%c",&y,&m,&d,&h,&n,&tail)!=5)return false;
    if(y<1601||y>9998||m<1||m>12||d<1||d>daysInMonth(y,m)||h<0||h>23||n<0||n>59)return false;
    SYSTEMTIME local{};local.wYear=static_cast<WORD>(y);local.wMonth=static_cast<WORD>(m);local.wDay=static_cast<WORD>(d);local.wHour=static_cast<WORD>(h);local.wMinute=static_cast<WORD>(n);
    SYSTEMTIME converted{},back{};
    if(!TzSpecificLocalTimeToSystemTimeEx(nullptr,&local,&converted)||!SystemTimeToTzSpecificLocalTimeEx(nullptr,&converted,&back))return false;
    if(back.wYear!=local.wYear||back.wMonth!=local.wMonth||back.wDay!=local.wDay||back.wHour!=local.wHour||back.wMinute!=local.wMinute)return false;
    utc=systemSeconds(converted);return true;
}
std::wstring targetText(long long utc) {
    if(!utc)return L"";
    auto t=secondsSystem(utc);SYSTEMTIME local{};if(!SystemTimeToTzSpecificLocalTimeEx(nullptr,&t,&local))return L"";
    wchar_t b[40]{};swprintf_s(b,L"%04u-%02u-%02u %02u:%02u",local.wYear,local.wMonth,local.wDay,local.wHour,local.wMinute);return b;
}
void rememberColor(std::vector<unsigned>& colors,unsigned color,size_t limit) {
    colors.erase(std::remove(colors.begin(),colors.end(),color),colors.end());colors.insert(colors.begin(),color&0xFFFFFF);
    if(colors.size()>limit)colors.resize(limit);
}
}
