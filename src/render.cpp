#include "render.hpp"
#include <fstream>
#include <cmath>

namespace dw {
static void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("Windows graphics operation failed"); }
Graphics::Graphics() {
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d));
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&text)));
    check(text->GetSystemFontCollection(&fonts));
    check(text->QueryInterface(__uuidof(IDWriteFactory5),reinterpret_cast<void**>(&modern)));
    check(modern->CreateInMemoryFontFileLoader(&memoryLoader));check(text->RegisterFontFileLoader(memoryLoader));
    IDWriteFontSetBuilder1* builder{};check(modern->CreateFontSetBuilder(&builder));
    for(int id=101;id<=107;++id){
        auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(id),RT_RCDATA);
        if(!resource){release(builder);throw std::runtime_error("Bundled font resource missing");}
        auto data=LockResource(LoadResource(nullptr,resource));auto bytes=SizeofResource(nullptr,resource);
        IDWriteFontFile* file{};check(memoryLoader->CreateInMemoryFontFileReference(text,data,bytes,nullptr,&file));
        auto hr=builder->AddFontFile(file);release(file);check(hr);
    }
    IDWriteFontSet* set{};check(builder->CreateFontSet(&set));release(builder);
    auto hr=modern->CreateFontCollectionFromFontSet(set,&bundled);release(set);check(hr);
}
Graphics::~Graphics() { release(bundled);if(memoryLoader)text->UnregisterFontFileLoader(memoryLoader);release(memoryLoader);release(modern);release(fonts); release(text); release(d2d); }
std::wstring Graphics::resolveFont(const std::wstring& requested) const {
    // These Windows face names belong to the Segoe UI family in DirectWrite.
    if(requested==L"Segoe UI Light" || requested==L"Segoe UI Semilight") return requested;
    for(auto family:BundledFontFamilies)for(auto suffix:{L"",L" Light",L" Semibold"})if(requested==std::wstring(family)+suffix)return requested;
    UINT32 index{}; BOOL exists{};
    if (SUCCEEDED(fonts->FindFamilyName(requested.c_str(), &index, &exists)) && exists) return requested;
    return L"Segoe UI";
}
Surface::Surface(Graphics& g) : graphics(g) { dc = CreateCompatibleDC(nullptr); if(!dc) throw std::runtime_error("Cannot create drawing DC"); }
Surface::~Surface() {
    clearFormats(); release(brush); release(target);
    if(previous) SelectObject(dc, previous);
    if(bitmap) DeleteObject(bitmap);
    if(dc) DeleteDC(dc);
}
void Surface::clearFormats() { for(auto& entry : formats) release(entry.second); formats.clear(); }
void Surface::ensure(SIZE size) {
    if (size.cx == pixels.cx && size.cy == pixels.cy && target) return;
    release(brush); release(target);
    if (previous) { SelectObject(dc, previous); previous = nullptr; }
    if (bitmap) { DeleteObject(bitmap); bitmap = nullptr; }
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size.cx; info.bmiHeader.biHeight = -size.cy;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* bits{};
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap) throw std::runtime_error("Cannot allocate widget surface");
    previous = SelectObject(dc, bitmap); pixels = size;
    auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    check(graphics.d2d->CreateDCRenderTarget(&properties, &target));
    RECT bounds{0,0,size.cx,size.cy}; check(target->BindDC(dc, &bounds));
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    check(target->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &brush));
}
void Surface::setColor(unsigned c, float a) { brush->SetColor(D2D1::ColorF(((c>>16)&255)/255.f, ((c>>8)&255)/255.f, (c&255)/255.f, a)); }
void Surface::text(const std::wstring& value, const std::wstring& family, float size, D2D1_RECT_F box, unsigned color, bool centered) {
    const auto font = graphics.resolveFont(family);
    std::wstring key = font + L"|" + std::to_wstring(static_cast<int>(size*100)) + (centered ? L"C" : L"L");
    auto it = formats.find(key);
    if (it == formats.end()) {
        IDWriteTextFormat* format{};
        auto familyName=font;
        auto weight=DWRITE_FONT_WEIGHT_NORMAL;
        if(font==L"Segoe UI Light") {familyName=L"Segoe UI";weight=DWRITE_FONT_WEIGHT_LIGHT;}
        else if(font==L"Segoe UI Semilight") {familyName=L"Segoe UI";weight=DWRITE_FONT_WEIGHT_SEMI_LIGHT;}
        IDWriteFontCollection* collection=nullptr;
        for(auto family:BundledFontFamilies)if(font==family||font==std::wstring(family)+L" Light"||font==std::wstring(family)+L" Semibold"){
            familyName=family;collection=graphics.bundled;
            if(font.ends_with(L" Light"))weight=DWRITE_FONT_WEIGHT_LIGHT;
            else if(font.ends_with(L" Semibold"))weight=DWRITE_FONT_WEIGHT_SEMI_BOLD;
        }
        check(graphics.text->CreateTextFormat(familyName.c_str(), collection, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &format));
        if(collection){IDWriteTextFormat3* variable{};if(SUCCEEDED(format->QueryInterface(__uuidof(IDWriteTextFormat3),reinterpret_cast<void**>(&variable)))){
            DWRITE_FONT_AXIS_VALUE axes[]{{DWRITE_FONT_AXIS_TAG_WEIGHT,static_cast<float>(weight)}};check(variable->SetFontAxisValues(axes,1));release(variable);}}
        format->SetTextAlignment(centered ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};IDWriteInlineObject* ellipsis{};
        if(SUCCEEDED(graphics.text->CreateEllipsisTrimmingSign(format,&ellipsis))){format->SetTrimming(&trim,ellipsis);release(ellipsis);}
        it = formats.emplace(key, format).first;
    }
    setColor(color);
    target->DrawText(value.c_str(), static_cast<UINT32>(value.size()), it->second, box, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Surface::draw(HWND window, const WidgetConfig& requested, const Config& c, const SYSTEMTIME& now, bool editing, bool present, UINT forcedDpi, const MediaSnapshot* media, const GlassTile* glass) {
    WidgetConfig w=requested;const bool material=c.preset==4||c.preset==5;
    HIGHCONTRASTW contrast{sizeof(HIGHCONTRASTW),0,nullptr};
    const bool highContrast=material&&SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0)&&(contrast.dwFlags&HCF_HIGHCONTRASTON);
    auto systemColor=[](int id){auto color=GetSysColor(id);return static_cast<unsigned>(GetRValue(color)<<16|GetGValue(color)<<8|GetBValue(color));};
    if(highContrast){w.color=systemColor(COLOR_WINDOWTEXT);w.accent=systemColor(COLOR_WINDOWTEXT);}
    if(c.preset==5&&!highContrast&&!w.customColors&&w.accent==Themes[5].accent&&((w.color>>16&255)*299+(w.color>>8&255)*587+(w.color&255)*114)>160000)w.accent=0xF6D96B;
    const UINT dpi = forcedDpi ? forcedDpi : GetDpiForWindow(window);
    auto logical = logicalSize(w.kind, c.preset, w.scale, c.seconds);
    SIZE size{MulDiv(logical.cx, dpi,96), MulDiv(logical.cy,dpi,96)};
    ensure(size);
    target->BeginDraw();
    target->SetTransform(D2D1::Matrix3x2F::Identity()); target->Clear(D2D1::ColorF(0,0,0,0));
    const float factor = dpi / 96.f * w.scale / 100.f;
    target->SetTransform(D2D1::Matrix3x2F::Scale(factor, factor));
    const auto base = logicalSize(w.kind, c.preset,100,c.seconds);
    const float width = static_cast<float>(base.cx), height = static_cast<float>(base.cy);
    if (editing || material || w.backing || w.kind==Kind::Music) {
        const auto panel=D2D1::RoundedRect(D2D1::RectF(1,1,width-1,height-1),c.preset==5?16.f:12.f,c.preset==5?16.f:12.f);
        if(material){
            if(!highContrast&&glass&&glass->width&&glass->height&&glass->pixels.size()==static_cast<size_t>(glass->width)*glass->height){
                ID2D1Bitmap* backdrop{};ID2D1RoundedRectangleGeometry* clip{};
                auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
                if(SUCCEEDED(target->CreateBitmap(D2D1::SizeU(glass->width,glass->height),glass->pixels.data(),glass->width*4,properties,&backdrop))&&SUCCEEDED(graphics.d2d->CreateRoundedRectangleGeometry(panel,&clip))){
                    target->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),clip),nullptr);target->DrawBitmap(backdrop,D2D1::RectF(1,1,width-1,height-1));target->PopLayer();
                }release(clip);release(backdrop);
            }
            const bool lightText=((w.color>>16&255)*299+(w.color>>8&255)*587+(w.color&255)*114)>160000;
            // Tagme uses a clearer white/cream material with a bright rim; Soft
            // retains the denser frost. Both use cached wallpaper, never screen capture.
            setColor(highContrast?systemColor(COLOR_WINDOW):lightText?0x182029:c.preset==5?0xFFFDF5:0xFFF9E9,highContrast?1.f:w.backing?.9f:c.preset==5?.60f:.74f);
            target->FillRoundedRectangle(panel,brush);
            if(c.preset==5&&!highContrast){
                D2D1_GRADIENT_STOP stops[]{{0,D2D1::ColorF(1,1,1,lightText?.08f:.26f)},{.5f,D2D1::ColorF(1,.99f,.96f,.025f)},{1,D2D1::ColorF(1,.94f,.71f,lightText?.035f:.12f)}};
                ID2D1GradientStopCollection* collection{};ID2D1LinearGradientBrush* sheen{};
                if(SUCCEEDED(target->CreateGradientStopCollection(stops,3,&collection))&&SUCCEEDED(target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0,0),D2D1::Point2F(width,height)),collection,&sheen)))target->FillRoundedRectangle(panel,sheen);
                release(sheen);release(collection);
                setColor(0xFFFFFF,lightText?.12f:.24f);target->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(3,3,width-3,height-3),14,14),brush,1);
            }
            setColor(highContrast?w.color:0xFFFFFF,highContrast?1.f:lightText?.22f:c.preset==5?.85f:.58f);
            target->DrawRoundedRectangle(panel,brush,1);
            setColor(0xFFFFFF,lightText?.28f:.65f);
            target->DrawLine(D2D1::Point2F(15,2),D2D1::Point2F(width-15,2),brush,1);
        }else if(w.kind==Kind::Music){
            setColor(0x18211D,editing?.55f:.13f);
            target->FillRoundedRectangle(panel,brush);
            setColor(w.accent,editing?.55f:.22f);
            target->DrawRoundedRectangle(panel,brush,1);
        }else{
            setColor(0x15231D,editing ? .72f : w.backing ? .84f : .65f);
            target->FillRoundedRectangle(panel,brush);
            setColor(w.accent,.8f);
            target->DrawRoundedRectangle(panel,brush,1);
        }
    }
    if(w.kind==Kind::Music) {
        MediaSnapshot empty;
        const auto& state=media?*media:empty;
        const bool hasTrack=state.hasSession&&!state.title.empty();
        const std::wstring label=state.hasSession?state.source:L"NOW PLAYING";
        const std::wstring title=hasTrack?state.title:state.apiAvailable?L"Nothing playing":L"Media controls unavailable";
        const std::wstring artist=hasTrack?(state.artist.empty()?L"Playing now":state.artist):L"Start audio in a supported app";
        text(label,w.font,12,D2D1::RectF(18,11,width-118,34),w.accent,false);
        text(title,w.font,21,D2D1::RectF(18,35,width-118,76),w.color,false);
        text(artist,w.font,12,D2D1::RectF(18,76,width-118,98),w.color,false);

        const D2D1_RECT_F cover=D2D1::RectF(width-100,12,width-20,92);
        if(state.thumbnailWidth&&state.thumbnailHeight&&state.thumbnail.size()==static_cast<size_t>(state.thumbnailWidth)*state.thumbnailHeight*4){
            ID2D1Bitmap* art{};
            D2D1_BITMAP_PROPERTIES properties{};properties.pixelFormat=D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED);properties.dpiX=properties.dpiY=96;
            if(SUCCEEDED(target->CreateBitmap(D2D1::SizeU(state.thumbnailWidth,state.thumbnailHeight),state.thumbnail.data(),state.thumbnailWidth*4,properties,&art))){
                target->DrawBitmap(art,cover,1.f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);release(art);
            }
        }else{
            setColor(w.accent,.16f);target->FillRoundedRectangle(D2D1::RoundedRect(cover,9,9),brush);
            setColor(w.accent,.62f);target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(width-60,51),18,18),brush,1.5f);
            target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(width-60,51),3,3),brush);
            target->DrawLine(D2D1::Point2F(width-47,38),D2D1::Point2F(width-47,57),brush,2);
            target->DrawLine(D2D1::Point2F(width-47,38),D2D1::Point2F(width-40,35),brush,2);
        }
        auto triangle=[&](float cx,float cy,bool right,unsigned color){
            ID2D1PathGeometry* path{};ID2D1GeometrySink* sink{};
            if(SUCCEEDED(graphics.d2d->CreatePathGeometry(&path))&&SUCCEEDED(path->Open(&sink))){
                const float direction=right?1.f:-1.f;
                sink->BeginFigure(D2D1::Point2F(cx-direction*5,cy-7),D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(cx+direction*6,cy));sink->AddLine(D2D1::Point2F(cx-direction*5,cy+7));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();setColor(color);target->FillGeometry(path,brush);release(sink);release(path);
            }else release(path);
        };
        const float cy=137.f;const unsigned active=w.color, muted=0x929B96;
        // Previous: bar and left-pointing triangle.
        setColor(state.canPrevious?active:muted,.92f);target->DrawLine(D2D1::Point2F(151,cy-7),D2D1::Point2F(151,cy+7),brush,2);
        triangle(160,cy,false,state.canPrevious?active:muted);
        // Play/pause uses the session state supplied by Windows.
        if(state.playing){
            setColor(state.canToggle?active:muted,.96f);target->FillRectangle(D2D1::RectF(224,cy-8,229,cy+8),brush);target->FillRectangle(D2D1::RectF(234,cy-8,239,cy+8),brush);
        }else triangle(231,cy,true,state.canToggle?active:muted);
        // Next: right-pointing triangle and bar.
        triangle(300,cy,true,state.canNext?active:muted);setColor(state.canNext?active:muted,.92f);target->DrawLine(D2D1::Point2F(309,cy-7),D2D1::Point2F(309,cy+7),brush,2);
        text(state.playing?L"PAUSE":L"PLAY",w.font,9,D2D1::RectF(209,153,253,172),w.accent);
    } else if (w.kind == Kind::Clock) {
        if(c.preset==3){
            auto time=clockText(now,c.hour24,c.seconds);
            text(time.substr(0,2),w.font,144,D2D1::RectF(6,0,width-6,162),w.color);
            text(time.substr(3,2),w.font,144,D2D1::RectF(6,151,width-6,310),w.accent);
            if(c.seconds)text(time.substr(6,2),w.font,19,D2D1::RectF(12,310,width-12,338),w.color);
        } else if(c.preset==2){
            text(clockText(now,c.hour24,c.seconds),w.font,c.seconds?85.f:108.f,D2D1::RectF(12,0,width-12,height-36),w.color,false);
            text(L"MAKE TIME",w.font,12,D2D1::RectF(18,height-36,width-18,height-6),w.accent,false);
        } else if (c.preset == 0) {
            const auto time = clockText(now,c.hour24,c.seconds);
            const float numberSize = c.seconds ? 124.f : 154.f;
            const float cell = c.seconds ? 204.f : 230.f;
            const float start = (width - cell * (c.seconds ? 3 : 2))/2;
            text(time.substr(0,2),w.font,numberSize,D2D1::RectF(start,0,start+cell,height-8),w.color);
            text(time.substr(3,2),w.font,numberSize,D2D1::RectF(start+cell,0,start+2*cell,height-8),w.accent);
            if(c.seconds) text(time.substr(6,2),w.font,numberSize,D2D1::RectF(start+2*cell,0,start+3*cell,height-8),w.color);
        } else {
            text(clockText(now,c.hour24,c.seconds),w.font,100,D2D1::RectF(8,0,width-8,height-12),w.color);
            setColor(w.accent,.85f);
            target->DrawLine(D2D1::Point2F(width*.35f,height-14),D2D1::Point2F(width*.65f,height-14),brush,2);
        }
        if(!c.hour24) text(now.wHour < 12 ? L"AM" : L"PM",w.font,11,D2D1::RectF(width-44,height-30,width-8,height-10),w.accent);
    } else if(w.kind == Kind::Date) {
        wchar_t date[180]{};
        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&now,L"dddd  '|'  MMMM dd yyyy",date,180,nullptr);
        text(date,w.font,27,D2D1::RectF(8,6,width-8,height-8),w.color);
    } else if(w.kind == Kind::Calendar) {
        wchar_t month[100]{};
        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&now,L"MMMM",month,100,nullptr);
        text(month,w.font,46,D2D1::RectF(12,4,width-12,65),w.accent);
        text(std::to_wstring(now.wYear),w.font,12,D2D1::RectF(10,63,width-10,83),w.color);
        static constexpr const wchar_t* names[]{L"Su",L"Mo",L"Tu",L"We",L"Th",L"Fr",L"Sa"};
        const float left=15, cell=(width-30)/7;
        for(int i=0;i<7;++i) text(names[i],w.font,12,D2D1::RectF(left+i*cell,92,left+(i+1)*cell,117),w.color);
        const int offset = firstWeekday(now.wYear, now.wMonth), count=daysInMonth(now.wYear,now.wMonth);
        for(int day=1; day<=count; ++day) {
            int slot=offset+day-1, col=slot%7, row=slot/7;
            const float x=left+col*cell,y=123+row*33.f;
            if(day==now.wDay) {
                setColor(w.accent,.18f); target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x+cell/2,y+16),14,14),brush);
                setColor(w.accent); target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x+cell/2,y+16),14,14),brush,1.2f);
            }
            text(std::to_wstring(day),w.font,14,D2D1::RectF(x,y,x+cell,y+32),day==now.wDay ? w.accent : w.color);
        }
    }
    else {
        std::wstring title,main,detail;float progress=-1;
        if(w.kind==Kind::Focus){
            const auto left=focusRemaining(c.focus,utcNow());wchar_t buffer[40]{};swprintf_s(buffer,L"%02lld:%02lld",left/60,left%60);main=buffer;
            title=c.focus.isBreak?L"BREAK":L"FOCUS";detail=c.focus.running?L"In progress":left==0?L"Session complete":L"Ready when you are";
            progress=1-static_cast<float>(left)/((c.focus.isBreak?c.focus.breakMinutes:c.focus.focusMinutes)*60);
        }else if(w.kind==Kind::Countdown){title=c.eventTitle;main=countdownText(c.eventUtc,utcNow());detail=c.eventUtc?targetText(c.eventUtc):L"Choose an event in Settings";}
        else if(w.kind==Kind::Battery){SYSTEM_POWER_STATUS power{};bool ok=GetSystemPowerStatus(&power)!=FALSE;title=L"BATTERY";main=batteryText(power,ok);
            if(ok&&power.BatteryFlag!=255&&!(power.BatteryFlag&128)&&power.BatteryLifePercent!=255){detail=(power.BatteryFlag&8)?L"Charging":power.ACLineStatus==1?L"Plugged in":L"On battery";progress=power.BatteryLifePercent/100.f;}}
        else {SYSTEMTIME utc{},local{};GetSystemTime(&utc);bool ok=zoneTime(c.zone,utc,local);title=c.city;main=clockText(local,c.hour24,false);
            SYSTEMTIME today=now,other=local;today.wHour=today.wMinute=today.wSecond=today.wMilliseconds=0;other.wHour=other.wMinute=other.wSecond=other.wMilliseconds=0;
            auto days=(systemSeconds(other)-systemSeconds(today))/86400;detail=days==0?L"Today":days>0?L"Tomorrow":L"Yesterday";
            if(!c.hour24)detail+=local.wHour<12?L" · AM":L" · PM";if(!ok)detail=L"UTC · zone unavailable";}
        text(title,w.font,14,D2D1::RectF(18,8,width-18,34),w.accent,false);
        text(main,w.font,w.kind==Kind::Countdown?29.f:48.f,D2D1::RectF(16,32,width-16,99),w.color,false);
        text(detail,w.font,12,D2D1::RectF(18,103,width-18,128),w.color,false);
        if(progress>=0){progress=std::clamp(progress,0.f,1.f);setColor(w.accent,.2f);target->DrawLine(D2D1::Point2F(18,140),D2D1::Point2F(width-18,140),brush,2);setColor(w.accent);target->DrawLine(D2D1::Point2F(18,140),D2D1::Point2F(18+(width-36)*progress,140),brush,2);}
    }
    const HRESULT hr=target->EndDraw();
    if(hr==D2DERR_RECREATE_TARGET) { release(brush); release(target); return; }
    check(hr);
    if(!present)return;
    POINT origin{}; SIZE dimensions=pixels; RECT rect{}; GetWindowRect(window,&rect);
    POINT position{rect.left,rect.top};
    BLENDFUNCTION blend{AC_SRC_OVER,0,static_cast<BYTE>(w.opacity*255/100),AC_SRC_ALPHA};
    if(!UpdateLayeredWindow(window,nullptr,&position,&dimensions,dc,&origin,0,&blend,ULW_ALPHA))
        throw std::runtime_error("Cannot update transparent widget window");
}
void Surface::preview(HDC destination,RECT box) const {
    if(!bitmap)return;
    float scale=std::min(static_cast<float>(box.right-box.left)/pixels.cx,static_cast<float>(box.bottom-box.top)/pixels.cy);
    int width=static_cast<int>(pixels.cx*scale),height=static_cast<int>(pixels.cy*scale);
    BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    GdiAlphaBlend(destination,box.left+(box.right-box.left-width)/2,box.top+(box.bottom-box.top-height)/2,width,height,dc,0,0,pixels.cx,pixels.cy,blend);
}
void Surface::saveBmp(const std::filesystem::path& path) const {
    BITMAP object{}; GetObject(bitmap,sizeof(object),&object);
    BITMAPFILEHEADER file{}; file.bfType=0x4d42; file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
    file.bfSize=file.bfOffBits+pixels.cx*pixels.cy*4;
    BITMAPINFOHEADER info{}; info.biSize=sizeof(info); info.biWidth=pixels.cx; info.biHeight=-pixels.cy;
    info.biPlanes=1; info.biBitCount=32; info.biSizeImage=pixels.cx*pixels.cy*4;
    std::ofstream stream(path,std::ios::binary);
    stream.write(reinterpret_cast<const char*>(&file),sizeof(file));
    stream.write(reinterpret_cast<const char*>(&info),sizeof(info));
    stream.write(static_cast<const char*>(object.bmBits),info.biSizeImage);
    if(!stream) throw std::runtime_error("Cannot export widget preview");
}
}
