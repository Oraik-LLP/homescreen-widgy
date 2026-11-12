#pragma once
#include "model.hpp"
#include <future>

namespace dw {
struct WallpaperImage {
    std::wstring device, path;
    RECT bounds{}, desktop{};
    unsigned background=0, accent=0;
    int position=4;
    UINT width=0,height=0,originalWidth=0,originalHeight=0;
    unsigned long long stamp=0;
    std::vector<unsigned> pixels;
    bool valid=false;
};
struct WallpaperResult { std::vector<WallpaperImage> images; std::wstring warning; unsigned decoded=0; size_t peakPrivate=0; };
WidgetConfig matchWallpaper(const WidgetConfig& widget,RECT screen,const WallpaperImage& image);
GlassTile frostedWallpaper(RECT screen,const WallpaperImage& image);
class WallpaperMatcher {
    std::future<WallpaperResult> pending;
    unsigned generation=0, launched=0;
    std::vector<Monitor> wanted;
    bool queued=false;
    void launch();
public:
    WallpaperResult result;
    unsigned totalDecoded=0;
    size_t peakPrivate=0;
    void request(const std::vector<Monitor>& monitors);
    void cancel();
    bool poll();
    bool busy() const {return pending.valid()||queued;}
};
}
