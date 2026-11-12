#pragma once
#include "model.hpp"
#include <d2d1.h>
#include <dwrite.h>
#include <map>
#include <memory>

namespace dw {
template<class T> void release(T*& p) { if(p) { p->Release(); p = nullptr; } }
class Graphics {
public:
    ID2D1Factory* d2d{};
    IDWriteFactory* text{};
    IDWriteFontCollection* fonts{};
    Graphics();
    ~Graphics();
    std::wstring resolveFont(const std::wstring& requested) const;
};
class Surface {
    Graphics& graphics;
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    ID2D1DCRenderTarget* target{};
    ID2D1SolidColorBrush* brush{};
    SIZE pixels{};
    std::map<std::wstring, IDWriteTextFormat*> formats;
    void ensure(SIZE size);
    void text(const std::wstring& value, const std::wstring& font, float size, D2D1_RECT_F box, unsigned color, bool centered = true);
    void setColor(unsigned color, float alpha = 1.0f);
public:
    explicit Surface(Graphics& graphics);
    ~Surface();
    Surface(const Surface&) = delete;
    void clearFormats();
    void draw(HWND window, const WidgetConfig& widget, const Config& config, const SYSTEMTIME& now, bool editing);
    void saveBmp(const std::filesystem::path& path) const;
    SIZE size() const { return pixels; }
};
}
