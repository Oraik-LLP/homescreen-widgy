#include "wallpaper.hpp"
#include <shobjidl.h>
#include <wincodec.h>
#include <psapi.h>
#include <cmath>
#include <map>
#include <chrono>
#include <wrl/client.h>

namespace dw {
using Microsoft::WRL::ComPtr;
static double luminance(unsigned c) {
    auto channel=[](unsigned v){double x=v/255.0;return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
    return .2126*channel(c>>16&255)+.7152*channel(c>>8&255)+.0722*channel(c&255);
}
static unsigned sample(const WallpaperImage& i,double sx,double sy) {
    if(i.pixels.empty())return i.background;
    const RECT r=i.position==5?i.desktop:i.bounds;
    double rw=r.right-r.left,rh=r.bottom-r.top,iw=i.originalWidth,ih=i.originalHeight;
    double x=sx-r.left,y=sy-r.top,scale=1,dx=0,dy=0;
    switch(i.position){
    case 0:dx=(rw-iw)/2;dy=(rh-ih)/2;break; // center
    case 1:x=std::fmod(std::max(0.0,sx-i.desktop.left),iw);y=std::fmod(std::max(0.0,sy-i.desktop.top),ih);break;
    case 2:x=x/rw*iw;y=y/rh*ih;break;
    default:scale=i.position==3?std::min(rw/iw,rh/ih):std::max(rw/iw,rh/ih);dx=(rw-iw*scale)/2;dy=(rh-ih*scale)/2;break;
    }
    if(i.position!=1&&i.position!=2){x=(x-dx)/scale;y=(y-dy)/scale;}
    if(x<0||y<0||x>=iw||y>=ih)return i.background;
    auto px=std::min(i.width-1,static_cast<UINT>(x/iw*i.width)),py=std::min(i.height-1,static_cast<UINT>(y/ih*i.height));
    return i.pixels[py*i.width+px];
}
WidgetConfig matchWallpaper(const WidgetConfig& w,RECT rect,const WallpaperImage& i) {
    auto matched=w;if(w.customColors||!i.valid)return matched;
    double sum=0,min=1,max=0;
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){double l=luminance(sample(i,rect.left+(x+.5)*(rect.right-rect.left)/8,rect.top+(y+.5)*(rect.bottom-rect.top)/8));sum+=l;min=std::min(min,l);max=std::max(max,l);}
    bool light=(1.05/(sum/64+.05))>((sum/64+.05)/.055);
    matched.color=light?0xF8F7F4:0x202520;
    matched.backing=(max-min>.28)||(light?1.05/(max+.05):(min+.05)/.055)<4.5;
    if(matched.backing)matched.color=0xF8F7F4;
    matched.accent=i.accent;
    double l=luminance(matched.accent),b=matched.backing?.025:sum/64;
    if((std::max(l,b)+.05)/(std::min(l,b)+.05)<3){
        unsigned mix=matched.color;unsigned c=matched.accent;
        matched.accent=(((c>>16&255)+(mix>>16&255))/2<<16)|(((c>>8&255)+(mix>>8&255))/2<<8)|((c&255)+(mix&255))/2;
    }
    return matched;
}
static WallpaperResult readWallpapers(std::vector<Monitor> monitors,const WallpaperResult& previous) {
    WallpaperResult out;HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    {
        ComPtr<IDesktopWallpaper> desktop;ComPtr<IWICImagingFactory> wic;
        HRESULT hr=CoCreateInstance(CLSID_DesktopWallpaper,nullptr,CLSCTX_ALL,IID_PPV_ARGS(&desktop));
        if(FAILED(hr)){out.warning=L"Wallpaper unavailable. Theme colors are active.";}
        else {
            CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic));
            DESKTOP_WALLPAPER_POSITION pos=DWPOS_FILL;desktop->GetPosition(&pos);COLORREF bg=0;desktop->GetBackgroundColor(&bg);
            UINT count=0;desktop->GetMonitorDevicePathCount(&count);
            RECT all{};bool first=true;
            for(UINT k=0;k<count;++k){LPWSTR id{};RECT r{};if(SUCCEEDED(desktop->GetMonitorDevicePathAt(k,&id))){desktop->GetMonitorRECT(id,&r);CoTaskMemFree(id);if(first){all=r;first=false;}else UnionRect(&all,&all,&r);}}
            for(const auto& monitor:monitors){
                WallpaperImage image;image.device=monitor.device;image.desktop=all;image.position=pos;
                MONITORINFO mi{};mi.cbSize=sizeof(mi);image.bounds=monitor.work;if(GetMonitorInfoW(monitor.handle,&mi))image.bounds=mi.rcMonitor;
                image.background=GetRValue(bg)<<16|GetGValue(bg)<<8|GetBValue(bg);
                bool found=false;
                for(UINT k=0;k<count;++k){LPWSTR id{};RECT r{};if(FAILED(desktop->GetMonitorDevicePathAt(k,&id)))continue;desktop->GetMonitorRECT(id,&r);
                    if(EqualRect(&r,&image.bounds)){LPWSTR path{};hr=desktop->GetWallpaper(id,&path);if(SUCCEEDED(hr)){image.path=path?path:L"";found=true;}CoTaskMemFree(path);}CoTaskMemFree(id);if(found)break;
                }
                if(!found){out.warning=L"Wallpaper unavailable on a display. Theme colors are active.";out.images.push_back(std::move(image));continue;}
                image.valid=true;image.accent=0x91B89D;
                if(!image.path.empty()){
                    WIN32_FILE_ATTRIBUTE_DATA data{};if(GetFileAttributesExW(image.path.c_str(),GetFileExInfoStandard,&data))image.stamp=(static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime)<<32)|data.ftLastWriteTime.dwLowDateTime;
                    auto old=std::find_if(previous.images.begin(),previous.images.end(),[&](const auto& p){return p.valid&&p.path==image.path&&p.stamp==image.stamp&&!p.pixels.empty();});
                    if(old!=previous.images.end()){image.width=old->width;image.height=old->height;image.originalWidth=old->originalWidth;image.originalHeight=old->originalHeight;image.pixels=old->pixels;image.accent=old->accent;}
                    else {
                        ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> converter;
                        hr=wic?wic->CreateDecoderFromFilename(image.path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder):E_FAIL;
                        if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
                        if(SUCCEEDED(hr))hr=frame->GetSize(&image.originalWidth,&image.originalHeight);
                        if(SUCCEEDED(hr)&&image.originalWidth&&image.originalHeight){double factor=std::min(1.0,128.0/std::max(image.originalWidth,image.originalHeight));image.width=std::max(1u,static_cast<UINT>(image.originalWidth*factor));image.height=std::max(1u,static_cast<UINT>(image.originalHeight*factor));hr=wic->CreateBitmapScaler(&scaler);}
                        else hr=E_FAIL;
                        if(SUCCEEDED(hr))hr=scaler->Initialize(frame.Get(),image.width,image.height,WICBitmapInterpolationModeFant);
                        if(SUCCEEDED(hr))hr=wic->CreateFormatConverter(&converter);
                        if(SUCCEEDED(hr))hr=converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
                        if(SUCCEEDED(hr)){image.pixels.resize(image.width*image.height);hr=converter->CopyPixels(nullptr,image.width*4,static_cast<UINT>(image.pixels.size()*4),reinterpret_cast<BYTE*>(image.pixels.data()));}
                        if(FAILED(hr)){image.valid=false;image.pixels.clear();out.warning=L"Wallpaper could not be read. Theme colors are active.";}
                        else {
                            ++out.decoded;std::map<unsigned,unsigned> buckets;
                            for(unsigned color:image.pixels)++buckets[color&0xF0F0F0];
                            double best=-1;for(auto [color,n]:buckets){double r=color>>16&255,g=color>>8&255,b=color&255,sat=(std::max({r,g,b})-std::min({r,g,b}))/255.;double score=n*(.2+sat);if(score>best){best=score;unsigned gray=static_cast<unsigned>((r+g+b)/3);image.accent=(static_cast<unsigned>(r*.65+gray*.35+12)<<16)|(static_cast<unsigned>(g*.65+gray*.35+12)<<8)|static_cast<unsigned>(b*.65+gray*.35+12);}}
                        }
                        PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)))out.peakPrivate=std::max(out.peakPrivate,static_cast<size_t>(memory.PrivateUsage));
                    }
                }
                out.images.push_back(std::move(image));
            }
        }
    }
    if(SUCCEEDED(initialized))CoUninitialize();return out;
}
void WallpaperMatcher::launch(){auto monitors=wanted;auto previous=result;launched=generation;queued=false;pending=std::async(std::launch::async,[monitors=std::move(monitors),previous=std::move(previous)]{return readWallpapers(monitors,previous);});}
void WallpaperMatcher::request(const std::vector<Monitor>& monitors){wanted=monitors;++generation;queued=true;if(!pending.valid())launch();}
void WallpaperMatcher::cancel(){++generation;queued=false;result={};}
bool WallpaperMatcher::poll(){
    if(!pending.valid()||pending.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
    WallpaperResult next;try{next=pending.get();}catch(...){next.warning=L"Wallpaper unavailable. Theme colors are active.";}
    totalDecoded+=next.decoded;peakPrivate=std::max(peakPrivate,next.peakPrivate);
    bool accepted=launched==generation;if(accepted)result=std::move(next);if(queued)launch();return accepted;
}
}
