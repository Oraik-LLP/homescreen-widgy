#include "../src/render.hpp"
#include <fstream>
#include <iostream>

static std::string bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int wmain(int argc,wchar_t** argv){
    using namespace dw;
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);int result=0;
    try {
        if(argc!=2)throw std::runtime_error("Pass a dedicated preview output directory");
        auto root=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(root);
        Graphics graphics;Surface surface(graphics);SYSTEMTIME now{};now.wYear=2026;now.wMonth=11;now.wDay=12;now.wHour=10;now.wMinute=8;now.wSecond=42;
        if(graphics.bundled->GetFontFamilyCount()!=3)throw std::runtime_error("Expected three private bundled families");
        int count=0;
        for(int theme=0;theme<ThemeCount;++theme)for(UINT dpi:{96u,144u,192u}){
            Config c;applyTheme(c,theme);c.eventTitle=L"A very long event title to verify graceful trimming";c.city=L"A very long world clock label to verify trimming";c.eventUtc=utcNow()+95000;
            for(int i=0;i<WidgetCount;++i){
                surface.draw(nullptr,c.widgets[i],c,now,false,false,dpi);
                auto path=root/(L"theme-"+std::to_wstring(theme)+L"-widget-"+std::to_wstring(i)+L"-dpi-"+std::to_wstring(dpi)+L".bmp");surface.saveBmp(path);++count;
                auto logical=logicalSize(static_cast<Kind>(i),theme,100,false);if(surface.size().cx!=MulDiv(logical.cx,dpi,96))throw std::runtime_error("Incorrect DPI-scaled surface width");
            }
        }
        for(int theme=0;theme<ThemeCount;++theme)for(bool h24:{false,true})for(bool seconds:{false,true}){
            Config c;applyTheme(c,theme);c.hour24=h24;c.seconds=seconds;now.wHour=h24?23:0;
            surface.draw(nullptr,c.widgets[0],c,now,false,false,144);surface.saveBmp(root/(L"clock-"+std::to_wstring(theme)+L"-24h-"+std::to_wstring(h24)+L"-seconds-"+std::to_wstring(seconds)+L".bmp"));++count;
        }
        for(auto family:{L"Inter",L"Manrope",L"DM Sans"}){
            Config c;c.widgets[0].font=std::wstring(family)+L" Light";surface.draw(nullptr,c.widgets[0],c,now,false,false,96);surface.saveBmp(root/L"light.bmp");
            c.widgets[0].font=std::wstring(family)+L" Semibold";surface.draw(nullptr,c.widgets[0],c,now,false,false,96);surface.saveBmp(root/L"semibold.bmp");
            if(bytes(root/L"light.bmp")==bytes(root/L"semibold.bmp"))throw std::runtime_error("Variable font weights render identically");
        }
        std::cout<<count<<" theme/widget/DPI previews exported; three private families and distinct light/semibold weights verified\n";
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';result=1;}
    if(SUCCEEDED(initialized))CoUninitialize();return result;
}
