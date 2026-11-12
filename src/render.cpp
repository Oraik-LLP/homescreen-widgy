#include "render.hpp"
#include <fstream>
#include <cmath>

namespace dw {
static void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("Windows graphics operation failed"); }
Graphics::Graphics() {
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d));
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&text)));
    check(text->GetSystemFontCollection(&fonts));
}
Graphics::~Graphics() { release(fonts); release(text); release(d2d); }
std::wstring Graphics::resolveFont(const std::wstring& requested) const {
    // These Windows face names belong to the Segoe UI family in DirectWrite.
    if(requested==L"Segoe UI Light" || requested==L"Segoe UI Semilight") return requested;
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
        check(graphics.text->CreateTextFormat(familyName.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &format));
        format->SetTextAlignment(centered ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        it = formats.emplace(key, format).first;
    }
    setColor(color);
    target->DrawText(value.c_str(), static_cast<UINT32>(value.size()), it->second, box, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Surface::draw(HWND window, const WidgetConfig& w, const Config& c, const SYSTEMTIME& now, bool editing) {
    const UINT dpi = GetDpiForWindow(window);
    auto logical = logicalSize(w.kind, c.preset, w.scale, c.seconds);
    SIZE size{MulDiv(logical.cx, dpi,96), MulDiv(logical.cy,dpi,96)};
    ensure(size);
    target->BeginDraw();
    target->SetTransform(D2D1::Matrix3x2F::Identity()); target->Clear(D2D1::ColorF(0,0,0,0));
    const float factor = dpi / 96.f * w.scale / 100.f;
    target->SetTransform(D2D1::Matrix3x2F::Scale(factor, factor));
    const auto base = logicalSize(w.kind, c.preset,100,c.seconds);
    const float width = static_cast<float>(base.cx), height = static_cast<float>(base.cy);
    if (editing) {
        setColor(0x15231D, .72f);
        target->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(1,1,width-1,height-1),12,12), brush);
        setColor(w.accent,.8f);
        target->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(1,1,width-1,height-1),12,12),brush,1);
    }
    if (w.kind == Kind::Clock) {
        if (c.preset == 0) {
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
        if(!c.hour24) text(now.wHour < 12 ? L"AM" : L"PM",L"Segoe UI",11,D2D1::RectF(width-44,height-30,width-8,height-10),w.accent);
    } else if(w.kind == Kind::Date) {
        wchar_t date[180]{};
        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&now,L"dddd  '|'  MMMM dd yyyy",date,180,nullptr);
        text(date,w.font,27,D2D1::RectF(8,6,width-8,height-8),w.color);
    } else {
        wchar_t month[100]{};
        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&now,L"MMMM",month,100,nullptr);
        text(month,w.font,46,D2D1::RectF(12,4,width-12,65),w.accent);
        text(std::to_wstring(now.wYear),L"Segoe UI",12,D2D1::RectF(10,63,width-10,83),w.color);
        static constexpr const wchar_t* names[]{L"Su",L"Mo",L"Tu",L"We",L"Th",L"Fr",L"Sa"};
        const float left=15, cell=(width-30)/7;
        for(int i=0;i<7;++i) text(names[i],L"Segoe UI",12,D2D1::RectF(left+i*cell,92,left+(i+1)*cell,117),w.color);
        const int offset = firstWeekday(now.wYear, now.wMonth), count=daysInMonth(now.wYear,now.wMonth);
        for(int day=1; day<=count; ++day) {
            int slot=offset+day-1, col=slot%7, row=slot/7;
            const float x=left+col*cell,y=123+row*33.f;
            if(day==now.wDay) {
                setColor(w.accent,.18f); target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x+cell/2,y+16),14,14),brush);
                setColor(w.accent); target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x+cell/2,y+16),14,14),brush,1.2f);
            }
            text(std::to_wstring(day),L"Segoe UI",14,D2D1::RectF(x,y,x+cell,y+32),day==now.wDay ? w.accent : w.color);
        }
    }
    const HRESULT hr=target->EndDraw();
    if(hr==D2DERR_RECREATE_TARGET) { release(brush); release(target); return; }
    check(hr);
    POINT origin{}; SIZE dimensions=pixels; RECT rect{}; GetWindowRect(window,&rect);
    POINT position{rect.left,rect.top};
    BLENDFUNCTION blend{AC_SRC_OVER,0,static_cast<BYTE>(w.opacity*255/100),AC_SRC_ALPHA};
    if(!UpdateLayeredWindow(window,nullptr,&position,&dimensions,dc,&origin,0,&blend,ULW_ALPHA))
        throw std::runtime_error("Cannot update transparent widget window");
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
