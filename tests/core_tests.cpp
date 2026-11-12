#include "../src/model.hpp"
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
        saveConfig(path,Config{});
        std::cout<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
