#include "../src/model.hpp"
#include "../src/wallpaper.hpp"
#include <fstream>
#include <iostream>

static int checks=0;
static void require(bool pass,const char* label){++checks;if(!pass)throw std::runtime_error(label);}
int wmain(int argc,wchar_t** argv) {
    using namespace dw;
    try {
        if(argc!=2)throw std::runtime_error("Pass a dedicated test output folder.");
        const auto root=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(root);
        const auto path=root/L"settings.json";
        require(leapYear(2000)&&leapYear(2024)&&!leapYear(1900)&&!leapYear(2025),"Gregorian leap-year rules");
        require(daysInMonth(2024,2)==29&&daysInMonth(2025,2)==28&&daysInMonth(2026,11)==30,"Month lengths");
        require(firstWeekday(2026,11)==0&&firstWeekday(2024,2)==4&&firstWeekday(2000,1)==6,"Calendar weekday alignment");
        SYSTEMTIME t{};t.wHour=0;t.wMinute=5;t.wSecond=9;t.wMilliseconds=345;
        require(clockText(t,false,false)==L"12:05"&&clockText(t,true,true)==L"00:05:09","Midnight clock formats");
        t.wHour=12;require(clockText(t,false,false)==L"12:05","Noon format");
        t.wHour=23;require(clockText(t,false,false)==L"11:05","Evening format");
        require(nextTick(t,true)==655&&nextTick(t,false)==50655,"Boundary-aligned timers");
        t.wSecond=59;t.wMilliseconds=999;require(nextTick(t,false)==1,"Minute rollover");
        auto p=clampPosition({-500,2000},{500,300},{-1920,0,0,1080});
        require(p.x==-500&&p.y==780,"Negative monitor coordinates and bottom clamp");
        p=clampPosition({20,30},{3000,2000},{0,0,1920,1080});require(p.x==0&&p.y==0,"Oversized widget remains reachable");
        p=snapPosition({-1894,38},{200,100},{-1920,0,0,1080},96);require(p.x==-1896&&p.y==48,"Grid uses monitor-relative coordinates on negative displays");
        p=snapPosition({-1870,70},{200,100},{-1920,0,0,1080},192);require(p.x==-1872&&p.y==48,"Grid spacing follows 200 percent DPI");
        p=snapPosition({-5,1040},{200,100},{-1920,0,0,1080},96);require(p.x==-200&&p.y==980,"Grid snapping keeps widgets inside the work area");
        unsigned color{};require(parseColor(L"#91B89D",color)&&color==0x91b89d,"Hex colors");
        require(!parseColor(L"#12345Z",color)&&!parseColor(L"#123",color),"Reject malformed colors");
        require(wide(utf8(L"日本語 — café"))==L"日本語 — café","Unicode round trip");
        Config config;config.seconds=true;config.hour24=false;config.preset=1;
        config.widgets[1].monitor=L"DISPLAY-日本";config.widgets[1].font=L"Consolas";
        config.widgets[1].scale=135;config.widgets[1].x=-123;config.widgets[2].visible=false;
        saveConfig(path,config);auto loaded=loadConfig(path);
        require(loaded.writable&&loaded.warning.empty()&&loaded.value.seconds&&!loaded.value.hour24,"Configuration flags round trip");
        require(loaded.value.widgets[1].monitor==L"DISPLAY-日本"&&loaded.value.widgets[1].scale==135&&loaded.value.widgets[1].x==-123&&!loaded.value.widgets[2].visible,"Widget settings round trip");
        {std::ofstream f(path);f<<R"({"version":1,"widgets":[{"id":0,"scale":9999,"opacity":-2,"x":9999999,"color":"oops"}]})";}
        loaded=loadConfig(path);require(loaded.value.widgets[0].scale==200&&loaded.value.widgets[0].opacity==10&&loaded.value.widgets[0].x==50000&&loaded.value.widgets[0].color==0xf2f4f1,"Clamp untrusted settings");
        {std::ofstream f(path);f<<R"({"version":99})";}
        loaded=loadConfig(path);require(!loaded.writable&&!loaded.warning.empty(),"Preserve future configuration versions");
        {std::ofstream f(path);f<<"{broken";}
        loaded=loadConfig(path);require(!loaded.writable&&!loaded.warning.empty(),"Preserve malformed configuration");
        {std::ifstream f(path);std::string data;f>>data;require(data=="{broken","Bad settings are never overwritten on load");}
        require(logicalSize(Kind::Clock,0,100,false).cx<logicalSize(Kind::Clock,0,100,true).cx,"Seconds receive more surface space");
        {std::ofstream f(path);f<<R"({"version":2,"preset":4,"widgets":[{"id":0,"x":77}]})";}
        loaded=loadConfig(path);require(!loaded.value.widgets[7].visible&&loaded.value.widgets[7].color==Themes[4].text&&loaded.value.widgets[0].x==77,"Older version 2 settings retain placement and give Music the active theme");
        Config expanded;applyTheme(expanded,3);expanded.widgets[3].visible=true;expanded.widgets[5].customColors=true;
        expanded.wallpaperMatch[3]=true;expanded.eventTitle=L"Launch — 日本";expanded.eventUtc=utcNow()+90000;
        expanded.city=L"New York";expanded.zone=L"Eastern Standard Time";expanded.focus.running=true;expanded.focus.deadline=utcNow()+500;
        expanded.recent={0x123456,0xABCDEF};expanded.favorites={0xFFFFFF};
        saveConfig(path,expanded);loaded=loadConfig(path);
        require(loaded.value.widgets.size()==8&&!loaded.value.widgets[4].visible&&loaded.value.widgets[3].visible&&!loaded.value.widgets[7].visible&&loaded.value.widgets[7].kind==Kind::Music,"Music appends stable ID 7 and starts hidden");
        require(loaded.value.wallpaperMatch[3]&&loaded.value.preset==3&&loaded.value.widgets[5].customColors,"Theme and manual colors persist");
        require(loaded.value.eventTitle==expanded.eventTitle&&loaded.value.eventUtc==expanded.eventUtc&&loaded.value.zone==expanded.zone,"Event and time zone round trip");
        require(loaded.value.focus.deadline==expanded.focus.deadline&&loaded.value.focus.running&&loaded.value.recent==expanded.recent&&loaded.value.favorites==expanded.favorites,"Timer deadline and palettes persist");
        expanded.widgets[0].x=123;expanded.widgets[0].scale=135;expanded.widgets[0].opacity=71;expanded.widgets[0].customColors=true;
        applyTheme(expanded,2);require(expanded.widgets[0].x==123&&expanded.widgets[0].scale==135&&expanded.widgets[0].opacity==71&&!expanded.widgets[0].customColors,"Theme changes retain placement, size and opacity");
        applyTheme(expanded,5);expanded.wallpaperMatch[5]=true;saveConfig(path,expanded);loaded=loadConfig(path);
        require(loaded.value.preset==5&&loaded.value.wallpaperMatch[5]&&loaded.value.widgets[0].x==123&&loaded.value.widgets[0].scale==135&&loaded.value.widgets[0].opacity==71,"Glass uses appended theme ID 5 and retains layout through a settings round trip");
        require(loaded.value.widgets[0].font==Themes[5].font&&std::wstring(Themes[4].name)==L"Soft","Glass typography applies while the existing Soft theme retains its ID");
        auto backupPath=std::filesystem::path(path.wstring()+L".v1.bak");std::string priorBackup;
        if(std::filesystem::exists(backupPath)){std::ifstream prior(backupPath);priorBackup.assign(std::istreambuf_iterator<char>(prior),{});}
        {std::ofstream f(path);f<<R"({"version":1,"preset":1,"widgets":[{"id":0,"x":37,"font":"Consolas","color":"#123456"}]})";}
        loaded=loadConfig(path);require(loaded.migrated&&loaded.value.widgets[0].font==L"Consolas"&&loaded.value.widgets[0].color==0x123456&&!loaded.value.widgets[3].visible,"Version 1 appearance is retained");
        saveConfig(path,loaded.value);require(std::filesystem::exists(path.wstring()+L".v1.bak"),"Version 1 backup is written before replacement");
        {std::ifstream f(std::filesystem::path(path.wstring()+L".v1.bak"));std::string data((std::istreambuf_iterator<char>(f)),{});require(priorBackup.empty()?data.find("\"version\":1")!=std::string::npos:data==priorBackup,"Migration backup retains original bytes");}
        {std::ofstream f(path);f<<R"({"version":2,"widgets":[{"id":999,"visible":true}],"favorites":["#123456","#123456","invalid"]})";}
        loaded=loadConfig(path);require(!loaded.value.widgets[6].visible&&loaded.value.favorites.size()==1,"Unknown widget IDs and invalid/duplicate colors are ignored");
        FocusState focus;focusAction(focus,0,1000);require(focus.running&&focus.deadline==2500,"Focus start stores an absolute deadline");
        focusAction(focus,0,1040);require(!focus.running&&focus.remaining==1460,"Focus pause retains remaining seconds");
        focusAction(focus,0,2000);require(focus.deadline==3460,"Focus resumes from remaining duration");
        require(finishFocus(focus,4000)&&focus.pending&&focus.remaining==0&&!focus.running,"Focus catches up after sleep or restart");
        require(!finishFocus(focus,4100),"Completed focus does not complete twice");
        focusAction(focus,2,4100);require(focus.isBreak&&!focus.running&&focus.remaining==300&&!focus.pending,"Next session waits for explicit start");
        require(countdownText(0,1000)==L"Set a date"&&countdownText(1000,1000)==L"Reached","Countdown unset and exact boundary");
        require(countdownText(1100,1000).starts_with(L"Today")&&countdownText(91000,1000).starts_with(L"1d"),"Countdown final day and days remaining");
        SYSTEM_POWER_STATUS power{};power.BatteryFlag=128;require(batteryText(power,true)==L"AC power","Desktop without battery");
        power.BatteryFlag=255;power.BatteryLifePercent=255;require(batteryText(power,true)==L"Unavailable"&&batteryText(power,false)==L"Unavailable","Unknown power status");
        power.BatteryFlag=8;power.BatteryLifePercent=42;require(batteryText(power,true)==L"42%","Battery percentage");
        SYSTEMTIME utc{};utc.wYear=2026;utc.wMonth=1;utc.wDay=15;utc.wHour=12;SYSTEMTIME zoned{};
        require(zoneTime(L"Eastern Standard Time",utc,zoned)&&zoned.wHour==7,"World clock winter standard time");
        utc.wMonth=7;require(zoneTime(L"Eastern Standard Time",utc,zoned)&&zoned.wHour==8,"World clock summer daylight saving");
        utc.wHour=2;require(zoneTime(L"Eastern Standard Time",utc,zoned)&&zoned.wDay==14,"World clock previous day");
        require(!zoneTime(L"missing-zone",utc,zoned)&&zoned.wHour==utc.wHour,"Unknown zone falls back to UTC");
        long long target{};require(!localTarget(L"2026-02-30 12:00",target)&&!localTarget(L"2026-01-01 25:00",target),"Invalid event dates are rejected");
        require(localTarget(L"2026-11-12 14:30",target)&&targetText(target)==L"2026-11-12 14:30","Local event target conversion round trip");
        std::vector<unsigned> colors;for(unsigned i=0;i<20;++i)rememberColor(colors,i,12);rememberColor(colors,15,12);
        require(colors.size()==12&&colors[0]==15&&std::count(colors.begin(),colors.end(),15)==1,"Recent colors are bounded and deduplicated");
        WallpaperImage image;image.valid=true;image.bounds={0,0,1000,800};image.desktop=image.bounds;image.accent=0x8899AA;
        image.background=0x112233;auto glass=frostedWallpaper({100,100,400,250},image);require(glass.width==64&&glass.height==48&&std::all_of(glass.pixels.begin(),glass.pixels.end(),[](unsigned p){return p==0xff112233;}),"Frosted solid wallpapers retain their color in a bounded tile");
        image.background=0;auto matched=matchWallpaper(Config{}.widgets[0],{100,100,400,250},image);require(matched.color==0xF8F7F4,"Dark wallpaper chooses light text");
        image.background=0xFFFFFF;matched=matchWallpaper(Config{}.widgets[0],{100,100,400,250},image);require(matched.color==0x202520,"Light wallpaper chooses dark text");
        image.width=8;image.height=8;image.originalWidth=1000;image.originalHeight=800;for(int i=0;i<64;++i)image.pixels.push_back(i%2?0:0xFFFFFF);
        matched=matchWallpaper(Config{}.widgets[0],{0,0,1000,800},image);require(matched.backing,"Busy wallpaper adds a contrast backing");
        auto clear=frostedWallpaper({0,0,1000,800},image,true);require(clear.width==64&&clear.height==48&&clear.pixels!=frostedWallpaper({0,0,1000,800},image).pixels,"Clear Glass and Soft frost generate distinct bounded backdrops");
        auto manual=Config{}.widgets[0];manual.customColors=true;manual.color=0x123456;require(matchWallpaper(manual,{0,0,1000,800},image).color==0x123456,"Manual colors override wallpaper matching");
        image.valid=false;manual.customColors=false;require(matchWallpaper(manual,{0,0,1000,800},image).color==0x123456,"Unreadable wallpaper retains theme colors");
        require(frostedWallpaper({0,0,1000,800},image).pixels.empty(),"Missing wallpaper uses translucent glass fallback");
        saveConfig(path,Config{});
        std::cout<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
